//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "HillCreepStressUpdate.h"
#include "ElasticityTensorTools.h"

registerMooseObject("SolidMechanicsApp", ADHillCreepStressUpdate);
registerMooseObject("SolidMechanicsApp", HillCreepStressUpdate);

template <bool is_ad>
InputParameters
HillCreepStressUpdateTempl<is_ad>::validParams()
{
  InputParameters params = AnisotropicReturnCreepStressUpdateBaseTempl<is_ad>::validParams();
  params.addClassDescription(
      "This class uses the stress update material in a generalized radial return anisotropic power "
      "law creep "
      "model.  This class can be used in conjunction with other creep and plasticity materials for "
      "more complex simulations.");

  // Linear strain hardening parameters
  params.addCoupledVar("temperature", "Coupled temperature");
  params.addRequiredParam<Real>("coefficient", "Leading coefficient in power-law equation");
  params.addRequiredParam<Real>("n_exponent", "Exponent on effective stress in power-law equation");
  params.addParam<Real>("m_exponent", 0.0, "Exponent on time in power-law equation");
  params.addRequiredParam<Real>("activation_energy", "Activation energy");
  params.addParam<Real>("gas_constant", 8.3143, "Universal gas constant");
  params.addParam<Real>("start_time", 0.0, "Start time (if not zero)");
  params.addParam<bool>("anisotropic_elasticity", false, "Enable using anisotropic elasticity");
  return params;
}

template <bool is_ad>
HillCreepStressUpdateTempl<is_ad>::HillCreepStressUpdateTempl(const InputParameters & parameters)
  : AnisotropicReturnCreepStressUpdateBaseTempl<is_ad>(parameters),
    _has_temp(this->isParamValid("temperature")),
    _temperature(this->_has_temp ? this->coupledValue("temperature") : this->_zero),
    _coefficient(this->template getParam<Real>("coefficient")),
    _n_exponent(this->template getParam<Real>("n_exponent")),
    _m_exponent(this->template getParam<Real>("m_exponent")),
    _activation_energy(this->template getParam<Real>("activation_energy")),
    _gas_constant(this->template getParam<Real>("gas_constant")),
    _start_time(this->template getParam<Real>("start_time")),
    _exponential(1.0),
    _exp_time(1.0),
    _hill_constants(this->template getMaterialPropertyByName<std::vector<Real>>(this->_base_name +
                                                                                "hill_constants")),
    _hill_tensor(this->_use_transformation
                     ? &this->template getMaterialPropertyByName<DenseMatrix<Real>>(
                           this->_base_name + "hill_tensor")
                     : nullptr),
    _qsigma(0.0),
    _C(6, 6),
    _elasticity_tensor_name(this->_base_name + "elasticity_tensor"),
    _elasticity_tensor(
        this->template getGenericMaterialProperty<RankFourTensor, is_ad>(_elasticity_tensor_name)),
    _anisotropic_elasticity(this->template getParam<bool>("anisotropic_elasticity"))
{
  if (_start_time < this->_app.getStartTime() && (std::trunc(_m_exponent) != _m_exponent))
    this->template paramError(
        "start_time",
        "Start time must be equal to or greater than the Executioner start_time if a "
        "non-integer m_exponent is used");
}

template <bool is_ad>
void
HillCreepStressUpdateTempl<is_ad>::computeStressInitialize(
    const GenericDenseVector<is_ad> & /*stress_dev*/,
    const GenericDenseVector<is_ad> & /*stress*/,
    const GenericRankFourTensor<is_ad> & elasticity_tensor)
{
  if (_has_temp)
    _exponential = std::exp(-_activation_energy / (_gas_constant * _temperature[_qp]));

  _two_shear_modulus = 2.0 * ElasticityTensorTools::getIsotropicShearModulus(elasticity_tensor);

  _exp_time = std::pow(_t - _start_time, _m_exponent);
}

template <bool is_ad>
GenericReal<is_ad>
HillCreepStressUpdateTempl<is_ad>::initialGuess(const GenericDenseVector<is_ad> & /*stress_dev*/)
{
  return 0.0;
}

template <bool is_ad>
GenericReal<is_ad>
HillCreepStressUpdateTempl<is_ad>::computeResidual(
    const GenericDenseVector<is_ad> & /*effective_trial_stress*/,
    const GenericDenseVector<is_ad> & stress_new,
    const GenericReal<is_ad> & delta_gamma)
{
  GenericReal<is_ad> qsigma_square;
  // Hill constants, some constraints apply
  const Real & F = _hill_constants[_qp][0];
  const Real & G = _hill_constants[_qp][1];
  const Real & H = _hill_constants[_qp][2];
  const Real & L = _hill_constants[_qp][3];
  const Real & M = _hill_constants[_qp][4];
  const Real & N = _hill_constants[_qp][5];

  if (!this->_use_transformation)
  {
    qsigma_square = F * (stress_new(1) - stress_new(2)) * (stress_new(1) - stress_new(2));
    qsigma_square += G * (stress_new(2) - stress_new(0)) * (stress_new(2) - stress_new(0));
    qsigma_square += H * (stress_new(0) - stress_new(1)) * (stress_new(0) - stress_new(1));
    qsigma_square += 2 * L * stress_new(4) * stress_new(4);
    qsigma_square += 2 * M * stress_new(5) * stress_new(5);
    qsigma_square += 2 * N * stress_new(3) * stress_new(3);
  }
  else
  {
    GenericDenseVector<is_ad> Ms(6);
    (*_hill_tensor)[_qp].vector_mult(Ms, stress_new);
    qsigma_square = Ms.dot(stress_new);
  }

  GenericReal<is_ad> qsigma = std::sqrt(qsigma_square);
  GenericReal<is_ad> qsigma_reduced;

  if (!_anisotropic_elasticity)
    qsigma_reduced = qsigma - 1.5 * _two_shear_modulus * delta_gamma;
  else
  {
    // Inelastic_strain_increment_tensor = plastic_multiplier x del(yield_function) /
    //                                     del(stress_tensor)  -  Based on normality hypothesis
    //
    // yield_function = effective_stress - yield_stress
    // del(yield_function) / del(stress_tensor) = del(effective_stress) / del(stress_tensor)
    // For isotropic cases, effective stress is vonMises stress and
    // for anisotropic cases the effective stress is hill_stress. We obtain:
    // del(yield_function) / del(stress_tensor) = b_vector / hill_stress
    // In the code, qsigma variable represents the hill_stress

    // delta_epsilon_inelastic is inelastic_strain_increment_tensor
    // delta_gamma is plastic_multiplier
    // f is yield_function
    // f = effective stress - yield stress.
    // We obtain:
    // delta_epsilon_inelastic = delta_gamma * del(f) / del(stress_tensor)
    // where del(f) / del(stress_tensor) = b_vector / hill_stress
    // Plugging value for del(f) / del(stress_tensor):
    // delta_epsilon_inelastic = delta_gamma * b_vector / hill_stress

    GenericRankTwoTensor<is_ad> stress;
    stress(0, 0) = stress_new(0);
    stress(1, 1) = stress_new(1);
    stress(2, 2) = stress_new(2);
    stress(0, 1) = stress(1, 0) = stress_new(3);
    stress(1, 2) = stress(2, 1) = stress_new(4);
    stress(0, 2) = stress(2, 0) = stress_new(5);

    GenericDenseVector<is_ad> b(6);
    b(0) = H * (stress(0, 0) - stress(1, 1)) - G * (stress(2, 2) - stress(0, 0));
    b(1) = F * (stress(1, 1) - stress(2, 2)) - H * (stress(0, 0) - stress(1, 1));
    b(2) = G * (stress(2, 2) - stress(0, 0)) - F * (stress(1, 1) - stress(2, 2));
    b(3) = 2.0 * N * stress(0, 1);
    b(4) = 2.0 * L * stress(1, 2);
    b(5) = 2.0 * M * stress(0, 2);

    GenericRankTwoTensor<is_ad> inelasticStrainIncrement;
    inelasticStrainIncrement(0, 0) = delta_gamma * b(0) / qsigma;
    inelasticStrainIncrement(1, 1) = delta_gamma * b(1) / qsigma;
    inelasticStrainIncrement(2, 2) = delta_gamma * b(2) / qsigma;
    inelasticStrainIncrement(0, 1) = inelasticStrainIncrement(1, 0) = delta_gamma * b(3) / qsigma;
    inelasticStrainIncrement(1, 2) = inelasticStrainIncrement(2, 1) = delta_gamma * b(4) / qsigma;
    inelasticStrainIncrement(0, 2) = inelasticStrainIncrement(2, 0) = delta_gamma * b(5) / qsigma;

    const GenericRankTwoTensor<is_ad> stress_reduced =
        stress - _elasticity_tensor[_qp] * inelasticStrainIncrement;

    GenericReal<is_ad> qsigma_square_reduced;
    qsigma_square_reduced = F * (stress_reduced(1, 1) - stress_reduced(2, 2)) *
                            (stress_reduced(1, 1) - stress_reduced(2, 2));
    qsigma_square_reduced += G * (stress_reduced(2, 2) - stress_reduced(0, 0)) *
                             (stress_reduced(2, 2) - stress_reduced(0, 0));
    qsigma_square_reduced += H * (stress_reduced(0, 0) - stress_reduced(1, 1)) *
                             (stress_reduced(0, 0) - stress_reduced(1, 1));
    qsigma_square_reduced += 2 * L * stress_reduced(1, 2) * stress_reduced(1, 2);
    qsigma_square_reduced += 2 * M * stress_reduced(0, 2) * stress_reduced(0, 2);
    qsigma_square_reduced += 2 * N * stress_reduced(0, 1) * stress_reduced(0, 1);
    qsigma_reduced = std::sqrt(qsigma_square_reduced);
  }

  const GenericReal<is_ad> creep_rate =
      _coefficient * std::pow(qsigma_reduced, _n_exponent) * _exponential * _exp_time;

  this->_inelastic_strain_rate[_qp] = MetaPhysicL::raw_value(creep_rate);
  // Return iteration difference between creep strain and inelastic strain multiplier
  return creep_rate * _dt - delta_gamma;
}

template <bool is_ad>
Real
HillCreepStressUpdateTempl<is_ad>::computeReferenceResidual(
    const GenericDenseVector<is_ad> & /*effective_trial_stress*/,
    const GenericDenseVector<is_ad> & /*stress_new*/,
    const GenericReal<is_ad> & /*residual*/,
    const GenericReal<is_ad> & /*scalar_effective_inelastic_strain*/)
{
  return 1.0;
}

template <bool is_ad>
GenericReal<is_ad>
HillCreepStressUpdateTempl<is_ad>::computeDerivative(
    const GenericDenseVector<is_ad> & /*effective_trial_stress*/,
    const GenericDenseVector<is_ad> & stress_new,
    const GenericReal<is_ad> & delta_gamma)
{
  GenericReal<is_ad> qsigma_square;
  GenericRankTwoTensor<is_ad> stress_reduced;
  // Hill constants, some constraints apply
  const Real & F = _hill_constants[_qp][0];
  const Real & G = _hill_constants[_qp][1];
  const Real & H = _hill_constants[_qp][2];
  const Real & L = _hill_constants[_qp][3];
  const Real & M = _hill_constants[_qp][4];
  const Real & N = _hill_constants[_qp][5];

  if (!this->_use_transformation)
  {
    // Equivalent deviatoric stress function.
    qsigma_square = F * (stress_new(1) - stress_new(2)) * (stress_new(1) - stress_new(2));
    qsigma_square += G * (stress_new(2) - stress_new(0)) * (stress_new(2) - stress_new(0));
    qsigma_square += H * (stress_new(0) - stress_new(1)) * (stress_new(0) - stress_new(1));
    qsigma_square += 2 * L * stress_new(4) * stress_new(4);
    qsigma_square += 2 * M * stress_new(5) * stress_new(5);
    qsigma_square += 2 * N * stress_new(3) * stress_new(3);
  }
  else
  {
    GenericDenseVector<is_ad> Ms(6);
    (*_hill_tensor)[_qp].vector_mult(Ms, stress_new);
    qsigma_square = Ms.dot(stress_new);
  }

  GenericReal<is_ad> qsigma = std::sqrt(qsigma_square);
  GenericReal<is_ad> qsigma_reduced;

  if (!_anisotropic_elasticity)
    qsigma_reduced = qsigma - 1.5 * _two_shear_modulus * delta_gamma;
  else
  {
    GenericRankTwoTensor<is_ad> stress;
    stress(0, 0) = stress_new(0);
    stress(1, 1) = stress_new(1);
    stress(2, 2) = stress_new(2);
    stress(0, 1) = stress(1, 0) = stress_new(3);
    stress(1, 2) = stress(2, 1) = stress_new(4);
    stress(0, 2) = stress(2, 0) = stress_new(5);

    GenericDenseVector<is_ad> b(6);
    b(0) = H * (stress(0, 0) - stress(1, 1)) - G * (stress(2, 2) - stress(0, 0));
    b(1) = F * (stress(1, 1) - stress(2, 2)) - H * (stress(0, 0) - stress(1, 1));
    b(2) = G * (stress(2, 2) - stress(0, 0)) - F * (stress(1, 1) - stress(2, 2));
    b(3) = 2.0 * N * stress(0, 1);
    b(4) = 2.0 * L * stress(1, 2);
    b(5) = 2.0 * M * stress(0, 2);

    GenericRankTwoTensor<is_ad> inelasticStrainIncrement;
    inelasticStrainIncrement(0, 0) = delta_gamma * b(0) / qsigma;
    inelasticStrainIncrement(1, 1) = delta_gamma * b(1) / qsigma;
    inelasticStrainIncrement(2, 2) = delta_gamma * b(2) / qsigma;
    inelasticStrainIncrement(0, 1) = inelasticStrainIncrement(1, 0) = delta_gamma * b(3) / qsigma;
    inelasticStrainIncrement(1, 2) = inelasticStrainIncrement(2, 1) = delta_gamma * b(4) / qsigma;
    inelasticStrainIncrement(0, 2) = inelasticStrainIncrement(2, 0) = delta_gamma * b(5) / qsigma;

    stress_reduced = stress - _elasticity_tensor[_qp] * inelasticStrainIncrement;

    GenericReal<is_ad> qsigma_square_reduced;
    qsigma_square_reduced = F * (stress_reduced(1, 1) - stress_reduced(2, 2)) *
                            (stress_reduced(1, 1) - stress_reduced(2, 2));
    qsigma_square_reduced += G * (stress_reduced(2, 2) - stress_reduced(0, 0)) *
                             (stress_reduced(2, 2) - stress_reduced(0, 0));
    qsigma_square_reduced += H * (stress_reduced(0, 0) - stress_reduced(1, 1)) *
                             (stress_reduced(0, 0) - stress_reduced(1, 1));
    qsigma_square_reduced += 2 * L * stress_reduced(1, 2) * stress_reduced(1, 2);
    qsigma_square_reduced += 2 * M * stress_reduced(0, 2) * stress_reduced(0, 2);
    qsigma_square_reduced += 2 * N * stress_reduced(0, 1) * stress_reduced(0, 1);
    qsigma_reduced = std::sqrt(qsigma_square_reduced);
  }

  _qsigma = qsigma_square; // this variable _qsigma is not used anywhere, should be removed

  // To calculate derivative of residual w.r.t delta_gamma we'll need del(f)/del(delta_gamma)
  // Using chain rule del(f)/del(delta_gamma) = del(f)/del(delta_epsilon_inelastic) *
  //                                            del(delta_epsilon_inelastic)/del(delta_gamma)

  // Evaluating del(f)/del(delta_epsilon_inelastic)
  // del(f)/del(dE_c_11) = (1/f) * {
  //                                F()

  //                               }
  //
  // S = C(E - dE_c) where S is stress, C is elasticity, E is total strain, E_c is creep strain
  // In component form it's written as:
  // |S_11|   |C11 C12 C13 2*C14 2*C15 2*C16| |E_11|   |C11 C12 C13 2*C14 2*C15 2*C16| |dE_c_11|
  // |S_22|   |C21 C22 C23 2*C24 2*C25 2*C26| |E_22|   |C21 C22 C23 2*C24 2*C25 2*C26| |dE_c_22|
  // |S_33| = |C31 C32 C33 2*C34 2*C35 2*C36| |E_33| - |C31 C32 C33 2*C34 2*C35 2*C36| |dE_c_33|
  // |S_12|   |C41 C42 C43 2*C44 2*C45 2*C46| |E_12|   |C41 C42 C43 2*C44 2*C45 2*C46| |dE_c_12|
  // |S_23|   |C51 C52 C53 2*C54 2*C55 2*C56| |E_23|   |C51 C52 C53 2*C54 2*C55 2*C56| |dE_c_23|
  // |S_13|   |C61 C62 C63 2*C64 2*C65 2*C66| |E_13|   |C61 C62 C63 2*C64 2*C65 2*C66| |dE_c_13|
  // Example:
  // del(S_12)/del(E_c_13) = - 2*C46 (element with row number of S_12 position in the S vector
  // and column number of E_c_13 position in the E_c vector)

  ElasticityTensorTools::toVoigtNotation<is_ad>(_C, _elasticity_tensor[_qp]);
  const unsigned int dimension = _C.n();
  GenericDenseMatrix<is_ad> d_stress_d_inelasticStrainIncrement(dimension, dimension);

  for (unsigned int index_i = 0; index_i < dimension; index_i++)
    for (unsigned int index_j = 0; index_j < dimension; index_j++)
    {
      if (index_j < 3)
        d_stress_d_inelasticStrainIncrement(index_i, index_j) =
            -1.0 * MetaPhysicL::raw_value(_C(index_i, index_j));
      else
        d_stress_d_inelasticStrainIncrement(index_i, index_j) =
            -2.0 * MetaPhysicL::raw_value(_C(index_i, index_j));
    }

  GenericDenseVector<is_ad> d_qsigma_d_inelasticStrainIncrement(6);
  for (unsigned int index_k = 0; index_k < 6; index_k++)
  {
    // d_qsigma_d_inelasticStrainIncrement(index_k) =
    //     F * (stress_new(1) - stress_new(2)) *
    //         (d_stress_d_inelasticStrainIncrement(1, index_k) -
    //          d_stress_d_inelasticStrainIncrement(2, index_k)) +
    //     G * (stress_new(2) - stress_new(0)) *
    //         (d_stress_d_inelasticStrainIncrement(2, index_k) -
    //          d_stress_d_inelasticStrainIncrement(0, index_k)) +
    //     H * (stress_new(0) - stress_new(1)) *
    //         (d_stress_d_inelasticStrainIncrement(0, index_k) -
    //          d_stress_d_inelasticStrainIncrement(1, index_k)) +
    //     2.0 * L * stress_new(4) * d_stress_d_inelasticStrainIncrement(4, index_k) +
    //     2.0 * M * stress_new(5) * d_stress_d_inelasticStrainIncrement(5, index_k) +
    //     2.0 * N * stress_new(3) * d_stress_d_inelasticStrainIncrement(3, index_k);
    // d_qsigma_d_inelasticStrainIncrement(index_k) /= qsigma;

    d_qsigma_d_inelasticStrainIncrement(index_k) =
        F * (stress_reduced(1, 1) - stress_reduced(2, 2)) *
            (d_stress_d_inelasticStrainIncrement(1, index_k) -
             d_stress_d_inelasticStrainIncrement(2, index_k)) +
        G * (stress_reduced(2, 2) - stress_reduced(0, 0)) *
            (d_stress_d_inelasticStrainIncrement(2, index_k) -
             d_stress_d_inelasticStrainIncrement(0, index_k)) +
        H * (stress_reduced(0, 0) - stress_reduced(1, 1)) *
            (d_stress_d_inelasticStrainIncrement(0, index_k) -
             d_stress_d_inelasticStrainIncrement(1, index_k)) +
        2.0 * L * stress_reduced(1, 2) * d_stress_d_inelasticStrainIncrement(4, index_k) +
        2.0 * M * stress_reduced(2, 0) * d_stress_d_inelasticStrainIncrement(5, index_k) +
        2.0 * N * stress_reduced(0, 1) * d_stress_d_inelasticStrainIncrement(3, index_k);
    d_qsigma_d_inelasticStrainIncrement(index_k) /= qsigma_reduced;
  }

  GenericDenseVector<is_ad> d_qsigma_d_sigma(6);
  // d_qsigma_d_sigma(0) =
  //     (H * (stress(0, 0) - stress(1, 1)) - G * (stress(2, 2) - stress(0, 0))) / qsigma;
  // d_qsigma_d_sigma(1) =
  //     (F * (stress(1, 1) - stress(2, 2)) - H * (stress(0, 0) - stress(1, 1))) / qsigma;
  // d_qsigma_d_sigma(2) =
  //     (G * (stress(2, 2) - stress(0, 0)) - F * (stress(1, 1) - stress(2, 2))) / qsigma;
  // d_qsigma_d_sigma(3) = 2.0 * N * stress(0, 1) / qsigma;
  // d_qsigma_d_sigma(4) = 2.0 * L * stress(1, 2) / qsigma;
  // d_qsigma_d_sigma(5) = 2.0 * M * stress(0, 2) / qsigma;

  d_qsigma_d_sigma(0) = (H * (stress_reduced(0, 0) - stress_reduced(1, 1)) -
                         G * (stress_reduced(2, 2) - stress_reduced(0, 0))) /
                        qsigma_reduced;
  d_qsigma_d_sigma(1) = (F * (stress_reduced(1, 1) - stress_reduced(2, 2)) -
                         H * (stress_reduced(0, 0) - stress_reduced(1, 1))) /
                        qsigma_reduced;
  d_qsigma_d_sigma(2) = (G * (stress_reduced(2, 2) - stress_reduced(0, 0)) -
                         F * (stress_reduced(1, 1) - stress_reduced(2, 2))) /
                        qsigma_reduced;
  d_qsigma_d_sigma(3) = 2.0 * N * stress_reduced(0, 1) / qsigma_reduced;
  d_qsigma_d_sigma(4) = 2.0 * L * stress_reduced(1, 2) / qsigma_reduced;
  d_qsigma_d_sigma(5) = 2.0 * M * stress_reduced(0, 2) / qsigma_reduced;

  GenericReal<is_ad> d_qsigma_d_deltaGamma =
      d_qsigma_d_inelasticStrainIncrement.dot(d_qsigma_d_sigma);

  const GenericReal<is_ad> creep_rate_derivative =
      _coefficient * d_qsigma_d_deltaGamma * _n_exponent *
      std::pow(qsigma_reduced, _n_exponent - 1.0) * _exponential * _exp_time;
  return (creep_rate_derivative * _dt - 1.0);
}

template <bool is_ad>
void
HillCreepStressUpdateTempl<is_ad>::computeStrainFinalize(
    GenericRankTwoTensor<is_ad> & inelasticStrainIncrement,
    const GenericRankTwoTensor<is_ad> & stress,
    const GenericDenseVector<is_ad> & stress_dev,
    const GenericReal<is_ad> & delta_gamma)
{
  GenericReal<is_ad> qsigma_square;
  if (!this->_use_transformation)
  {
    // Hill constants, some constraints apply
    const Real & F = _hill_constants[_qp][0];
    const Real & G = _hill_constants[_qp][1];
    const Real & H = _hill_constants[_qp][2];
    const Real & L = _hill_constants[_qp][3];
    const Real & M = _hill_constants[_qp][4];
    const Real & N = _hill_constants[_qp][5];

    // Equivalent deviatoric stress function.
    qsigma_square = F * (stress(1, 1) - stress(2, 2)) * (stress(1, 1) - stress(2, 2));
    qsigma_square += G * (stress(2, 2) - stress(0, 0)) * (stress(2, 2) - stress(0, 0));
    qsigma_square += H * (stress(0, 0) - stress(1, 1)) * (stress(0, 0) - stress(1, 1));
    qsigma_square += 2 * L * stress(1, 2) * stress(1, 2);
    qsigma_square += 2 * M * stress(0, 2) * stress(0, 2);
    qsigma_square += 2 * N * stress(0, 1) * stress(0, 1);
  }
  else
  {
    GenericDenseVector<is_ad> Ms(6);
    (*_hill_tensor)[_qp].vector_mult(Ms, stress_dev);
    qsigma_square = Ms.dot(stress_dev);
  }

  if (qsigma_square == 0)
  {
    inelasticStrainIncrement.zero();

    AnisotropicReturnCreepStressUpdateBaseTempl<is_ad>::computeStrainFinalize(
        inelasticStrainIncrement, stress, stress_dev, delta_gamma);

    this->_effective_inelastic_strain[_qp] = this->_effective_inelastic_strain_old[_qp];

    return;
  }

  // Use Hill-type flow rule to compute the time step inelastic increment.
  GenericReal<is_ad> prefactor = delta_gamma / std::sqrt(qsigma_square);

  if (!this->_use_transformation)
  {
    // Hill constants, some constraints apply
    const Real & F = _hill_constants[_qp][0];
    const Real & G = _hill_constants[_qp][1];
    const Real & H = _hill_constants[_qp][2];
    const Real & L = _hill_constants[_qp][3];
    const Real & M = _hill_constants[_qp][4];
    const Real & N = _hill_constants[_qp][5];

    inelasticStrainIncrement(0, 0) =
        prefactor * (H * (stress(0, 0) - stress(1, 1)) - G * (stress(2, 2) - stress(0, 0)));
    inelasticStrainIncrement(1, 1) =
        prefactor * (F * (stress(1, 1) - stress(2, 2)) - H * (stress(0, 0) - stress(1, 1)));
    inelasticStrainIncrement(2, 2) =
        prefactor * (G * (stress(2, 2) - stress(0, 0)) - F * (stress(1, 1) - stress(2, 2)));

    inelasticStrainIncrement(0, 1) = inelasticStrainIncrement(1, 0) =
        prefactor * 2.0 * N * stress(0, 1);
    inelasticStrainIncrement(0, 2) = inelasticStrainIncrement(2, 0) =
        prefactor * 2.0 * M * stress(0, 2);
    inelasticStrainIncrement(1, 2) = inelasticStrainIncrement(2, 1) =
        prefactor * 2.0 * L * stress(1, 2);
  }
  else
  {
    GenericDenseVector<is_ad> inelastic_strain_increment(6);
    GenericDenseVector<is_ad> Ms(6);
    (*_hill_tensor)[_qp].vector_mult(Ms, stress_dev);

    for (unsigned int i = 0; i < 6; i++)
      inelastic_strain_increment(i) = Ms(i) * prefactor;

    inelasticStrainIncrement(0, 0) = inelastic_strain_increment(0);
    inelasticStrainIncrement(1, 1) = inelastic_strain_increment(1);
    inelasticStrainIncrement(2, 2) = inelastic_strain_increment(2);

    inelasticStrainIncrement(0, 1) = inelasticStrainIncrement(1, 0) = inelastic_strain_increment(3);
    inelasticStrainIncrement(1, 2) = inelasticStrainIncrement(2, 1) = inelastic_strain_increment(4);
    inelasticStrainIncrement(0, 2) = inelasticStrainIncrement(2, 0) = inelastic_strain_increment(5);
  }

  AnisotropicReturnCreepStressUpdateBaseTempl<is_ad>::computeStrainFinalize(
      inelasticStrainIncrement, stress, stress_dev, delta_gamma);

  this->_effective_inelastic_strain[_qp] = this->_effective_inelastic_strain_old[_qp] + delta_gamma;
}

template <bool is_ad>
void
HillCreepStressUpdateTempl<is_ad>::computeStressFinalize(
    const GenericRankTwoTensor<is_ad> & creepStrainIncrement,
    const GenericReal<is_ad> & /*delta_gamma*/,
    GenericRankTwoTensor<is_ad> & stress_new,
    const GenericDenseVector<is_ad> & /*stress_dev*/,
    const GenericRankTwoTensor<is_ad> & stress_old,
    const GenericRankFourTensor<is_ad> & elasticity_tensor)
{
  // Class only valid for isotropic elasticity (for now)
  stress_new -= elasticity_tensor * creepStrainIncrement;

  // Compute the maximum time step allowed due to creep strain numerical integration error
  Real stress_dif = MetaPhysicL::raw_value(stress_new - stress_old).L2norm();

  // Get a representative value of the elasticity tensor
  Real elasticity_value =
      1.0 / 3.0 *
      MetaPhysicL::raw_value((elasticity_tensor(0, 0, 0, 0) + elasticity_tensor(1, 1, 1, 1) +
                              elasticity_tensor(2, 2, 2, 2)));

  if (std::abs(stress_dif) > TOLERANCE * TOLERANCE)
    this->_max_integration_error_time_step =
        _dt / (stress_dif / elasticity_value / this->_max_integration_error);
  else
    this->_max_integration_error_time_step = std::numeric_limits<Real>::max();
}

template class HillCreepStressUpdateTempl<false>;
template class HillCreepStressUpdateTempl<true>;
