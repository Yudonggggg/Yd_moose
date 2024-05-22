# This test uses the same creep model as ad_aniso_creep_x_3d.i
# and should therefore solve for the same creep strain values
# as in the other test. The method for computing residual and
# its derivative is different than in the other test: this test
# uses the method that allows using anisotropic elasticity. Since
# both tests simulate a uniaxial test, the creep strains should
# not differ which happens to be the case. The creep strain
# values obtained from this test verifies that the implementation
# of the code for computing residual and its derivative, while
# allowing anisotropic elasticity, is correct.

# time creep_strain_xx_from_ad_aniso_creep_x_3d.i creep_strain_xx_from_this_test
# 0      0                   0
# 0.0001 2.7406745871651e-07 2.7407731645306e-07
# 0.0002 5.4813491743301e-07 5.4815463290612e-07
# 0.0003 8.2220237614951e-07 8.2223194935918e-07
# 0.0004 1.096269834866e-06  1.0963092658122e-06
# 0.0005 1.3703372935825e-06 1.3703865822653e-06
# 0.0006 1.644404752299e-06  1.6444638987183e-06
# 0.0007 1.9184722110155e-06 1.9185412151714e-06
# 0.0008 2.192539669732e-06  2.1926185316244e-06
# 0.0009 2.4666071284485e-06 2.4666958480775e-06
# 0.0010 2.740674587165e-06  2.7407731645305e-06


[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 3
    nx = 10
    ny = 2
    nz = 2
    xmin = 0.0
    ymin = 0.0
    zmin = 0.0

    xmax = 10.0
    ymax = 1.0
    zmax = 1.0
  []
  [corner_node]
    type = ExtraNodesetGenerator
    new_boundary = '100'
    nodes = '3 69'
    input = gen
  []
  [corner_node_2]
    type = ExtraNodesetGenerator
    new_boundary = '101'
    nodes = '4 47'
    input = corner_node
  []
[]

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
  volumetric_locking_correction = true
[]

[AuxVariables]
  [hydrostatic_stress]
    order = CONSTANT
    family = MONOMIAL
  []
  [creep_strain_xx]
    order = CONSTANT
    family = MONOMIAL
  []
  [creep_strain_xy]
    order = CONSTANT
    family = MONOMIAL
  []
  [creep_strain_yy]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [hydrostatic_stress]
    type = ADRankTwoScalarAux
    variable = hydrostatic_stress
    rank_two_tensor = stress
    scalar_type = Hydrostatic
  []
  [creep_strain_xx]
    type = ADRankTwoAux
    rank_two_tensor = creep_strain
    variable = creep_strain_xx
    index_i = 0
    index_j = 0
  []
  [creep_strain_xy]
    type = ADRankTwoAux
    rank_two_tensor = creep_strain
    variable = creep_strain_xy
    index_i = 0
    index_j = 1
  []
  [creep_strain_yy]
    type = ADRankTwoAux
    rank_two_tensor = creep_strain
    variable = creep_strain_yy
    index_i = 1
    index_j = 1
  []
  [sigma_xx]
    type = ADRankTwoAux
    rank_two_tensor = stress
    variable = stress_xx
    index_i = 1
    index_j = 1
  []
[]

[Functions]
  [pull]
    type = PiecewiseLinear
    x = '0 1.0e-9 1.0'
    y = '0 -4e1 -4e1'
  []
[]

[Modules/TensorMechanics/Master]
  [all]
    strain = FINITE
    generate_output = 'elastic_strain_xx stress_xx'
    use_automatic_differentiation = true
    add_variables = true
  []
[]

[Materials]
  [elasticity_tensor]
    type = ADComputeIsotropicElasticityTensor
    youngs_modulus = 700
    poissons_ratio = 0.0
  []

  [elastic_strain]
    type = ADComputeMultipleInelasticStress
    inelastic_models = "trial_creep_two"
    max_iterations = 50
    absolute_tolerance = 1e-16
  []

  [hill_tensor]
    type = ADHillConstants
    # F G H L M N
    hill_constants = "0.5 0.25 0.3866 1.6413 1.6413 1.2731"
  []

  [trial_creep_two]
    type = ADHillCreepStressUpdate
    coefficient = 1e-16
    n_exponent = 9
    m_exponent = 0
    activation_energy = 0
    max_inelastic_increment = 0.00003
    absolute_tolerance = 1e-20
    relative_tolerance = 1e-20
    # Force it to not use integration error
    max_integration_error = 100.0
    anisotropic_elasticity = true
  []
[]

[BCs]
  [no_disp_x]
    type = ADDirichletBC
    variable = disp_x
    boundary = left
    value = 0.0
  []

  [no_disp_y]
    type = ADDirichletBC
    variable = disp_y
    boundary = 100
    value = 0.0
  []

  [no_disp_z]
    type = ADDirichletBC
    variable = disp_z
    boundary = 101
    value = 0.0
  []

  [Pressure]
    [Side1]
      boundary = right
      function = pull
    []
  []

[]

[Executioner]
  type = Transient

  solve_type = PJFNK
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_package -mat_mffd_err'
  petsc_options_value = 'lu     superlu_dist                    1e-5'

  nl_rel_tol = 1.0e-14
  nl_abs_tol = 1.0e-14
  l_max_its = 10
  num_steps = 5
  dt = 1.0e-4
  start_time = 0
  automatic_scaling = true
[]

[Postprocessors]
  [matl_ts_min]
    type = MaterialTimeStepPostprocessor
  []
  [max_disp_x]
    type = ElementExtremeValue
    variable = disp_x
  []
  [max_disp_y]
    type = ElementExtremeValue
    variable = disp_y
  []
  [max_hydro]
    type = ElementAverageValue
    variable = hydrostatic_stress
  []
  [dt]
    type = TimestepSize
  []
  [num_lin]
    type = NumLinearIterations
    outputs = console
  []
  [num_nonlin]
    type = NumNonlinearIterations
    outputs = console
  []
  [creep_strain_xx]
    type = ElementalVariableValue
    variable = creep_strain_xx
    execute_on = 'TIMESTEP_END'
    elementid = 39
  []
  [elastic_strain_xx]
    type = ElementalVariableValue
    variable = elastic_strain_xx
    execute_on = 'TIMESTEP_END'
    elementid = 39
  []
  [sigma_xx]
    type = ElementalVariableValue
    variable = stress_xx
    execute_on = 'TIMESTEP_END'
    elementid = 39
  []
[]

[Outputs]
  csv = true
  exodus = false
  perf_graph = true
  # unnecessary output variables
  hide = 'matl_ts_min max_disp_x max_disp_y max_hydro dt num_lin num_nonlin'
[]
