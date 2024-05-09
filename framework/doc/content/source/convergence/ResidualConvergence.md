# ResidualConvergence

The `ResidualConvergence` object facilitates the assesment of convergence properties of MOOSE applications and allows extensions to provide the user more control over the application behaviour. 


## Description

By default, MOOSE checks convergence using relative and absolute criteria. Once the residual drops
below either an absolute tolerance, or the residual divided by the initial residual for the current
time step drops below a relative tolerance, the solution is considered converged. 

## Example input syntax

!listing test/tests/convergence/diffusion_convergence.i block=Convergence

where the [!param](/Executioner/nonlinear_convergence) indicates the convergence type to be `ResidualConvergence` and additional parameters. Curently convergence specific parameters can be still specified in the Executioner block.


!syntax parameters /Convergence/ResidualConvergence

!syntax inputs /Convergence/ResidualConvergence

!syntax children /Convergence/ResidualConvergence
