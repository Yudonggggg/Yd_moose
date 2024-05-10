# Convergence system

The Convergence system provides the infrastracuture for creating MOOSe objects that interact with the 
solvers to give the user more control over the application behaviour. 

## Description

By default, MOOSE checks convergence using relative and absolute criteria. Once the residual drops
below either an absolute tolerance, or the residual divided by the initial residual for the current
time step drops below a relative tolerance, the solution is considered converged. This works well for
many problems, but there are some scenarios that are problematic, where the user may desire 
interaction with the solver at runtime for better control or analysis. 

Currently this object supports algebraic two types of convergence
[ResidualConvergence](syntax/Convergence/ResidualConvergence.md)
[ReferenceResidualConvergence](syntax/Convergence/ReferenceResidualConvergence.md)

### Methods supported

- +Residual Convergence+

This type of convergence [ResidualConvergence](syntax/Convergence/ResidualConvergence.md)

- +Reference Residual Convergence+
 [ReferenceResidualConvergence](syntax/Convergence/ReferenceResidualConvergence.md)

`ReferenceResidualConvergence` checks for convergence by comparing the residual to a different
reference quantity (instead of the initial residual). The user specifies a reference vector that can be used in
a relative convergence check. Because the solution variables can have significantly different
scaling, the convergence check performed in `ReferenceResidualConvergence` checks convergence of the
solution variables individually. When the $L^2$ norm of the residual for each solution variable is
less than either the relative tolerance times the $L^2$ norm of the corresponding reference variable
or the absolute tolerance, the solution is considered converged.

Since relative convergence is computed differently with this approach, the nonlinear relative
tolerance required to achieve the same error is typically different than with the default approach in
MOOSE, and the differences will vary by the problem. The code user must evaluate the behavior of
their model to ensure that appropriate tolerances are being used.

### Solver convergence criteria parameters

Parameters for setting absolute convergence, relative convergence etc. are usually set in the `[Executioner]` block

## Example input syntax

!listing test/tests/convergence/diffusion_convergence.i block=Convergence

where the [!param](/Convergence/type) indicates the convergence type to be used, which currently can be `ResidualConvergence` and `ReferenceResidualConvergence`.

!listing test/tests/convergence/diffusion_convergence.i block=Executioner
where the [!param](/Executioner/nonlinear_convergence) indicates the convergence object to be used.

!listing test/tests/convergence/reference_residual.i block=Convergence

!syntax parameters /Convergence

!syntax inputs /Convergence

!syntax children /Convergence
