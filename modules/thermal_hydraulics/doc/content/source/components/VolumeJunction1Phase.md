# VolumeJunction1Phase

This is a [flow junction](component_groups/flow_junction.md) that has a volume
and can connect 2 or more [FlowChannel1Phase.md] components in any orientation.

## Usage id=usage

!template load file=flow_junction_usage.md.template name=VolumeJunction1Phase

A form loss coefficient $K$ may be specified using the parameter
[!param](/Components/VolumeJunction1Phase/K).
The parameter [!param](/Components/VolumeJunction1Phase/A_ref) is the reference
cross-sectional area $A_\text{ref}$ used in [formloss_momentum] and [formloss_energy]. If it is
not provided, the cross-sectional area of the first connection in
[!param](/Components/VolumeJunction1Phase/connections) is used.

!alert note title=Connection order matters when using form loss
The order of connections in [!param](/Components/VolumeJunction1Phase/connections)
has an impact when using form loss, since some quantities in [formloss_momentum] and [formloss_energy]
are taken from the *first* connection.

!template load file=volume_junction_1phase_usage.md.template name=VolumeJunction1Phase

!syntax parameters /Components/VolumeJunction1Phase

## Formulation

A general conservation law equation can be expressed as

!equation
\pd{a}{t} + \nabla\cdot\mathbf{f} = 0 \eqc

where $a$ is the conserved quantity. Integrating this equation over a volume
$\Omega$ gives

!equation
\int\limits_\Omega \pd{a}{t} \,d\Omega + \int\limits_\Omega \nabla\cdot\mathbf{f} \,d\Omega = 0 \eqp

The time derivative can be expressed as follows:

!equation
\int\limits_\Omega {\pd{a}{t}} \,d\Omega = \ddt{\bar{a}V} \eqc

where $\bar{a}$ is the average of $a$ over $\Omega$:

!equation
\bar{a} \equiv \frac{1}{V}\int\limits_\Omega a \,d\Omega \eqc

and $V \equiv \int\limits_\Omega \,d\Omega$.

The flux term can be expressed as follows:

!equation
\int\limits_\Omega \nabla\cdot\mathbf{f} \,d\Omega = \int\limits_\Gamma \mathbf{f}\cdot\mathbf{n} \,d\Gamma \eqc

where $\Gamma$ is the surface of $\Omega$, and $\mathbf{n}$ is the outward
normal vector at a position on the surface.

Now consider that $\Gamma$ is partitioned into a number of regions:
$\Gamma = \Gamma_{\text{wall}} \cup \Gamma_{\text{flow}}$, where
$\Gamma_{\text{flow}} = \bigcup\limits_{i=1}^{N} \Gamma_{\text{flow},i}$.
The surface $\Gamma_{\text{wall}}$ corresponds to a wall, and the surface
$\Gamma_{\text{flow},i}$ corresponds to a flow surface, across which a fluid
may pass. The number of flow surfaces on the volume is denoted by $N$.
See [volume_junction] for an illustration.

!media thermal_hydraulics/tikz_diagrams/volume_junction.png
       id=volume_junction
       style=width:50%;display:block;margin-left:auto;margin-right:auto;
       caption=Volume junction illustration

Now the surface integral can be expressed as follows:

!equation
\int\limits_\Gamma \mathbf{f}\cdot\mathbf{n} \,d\Gamma =
    \int\limits_{\Gamma_{\text{wall}}} \mathbf{f}\cdot\mathbf{n} \,d\Gamma
    + \sum\limits_{i=1}^N \int\limits_{\Gamma_{\text{flow},i}}{\mathbf{f}\cdot\mathbf{n}} \,d\Gamma \eqp

Now consider that the flux $\mathbf{f}$ is constant over each flow surface, and that
each flow surface is flat (thus making $\mathbf{n}$ constant over the surface).
Then,

!equation
\int\limits_{\Gamma_{\text{flow},i}} \mathbf{f}\cdot\mathbf{n} \,d\Gamma
    = \mathbf{f}_i \cdot \mathbf{n}_{\text{J},i} A_i \eqc

where $\mathbf{n}_{\text{J},i} = -\mathbf{n}_i$, and $\mathbf{n}_i$ is the outward-facing
(from the channel perspective) normal of channel $i$ at the interface with the
junction.

Now the wall surface integral term needs to be discussed. At this point, we
consider the particular conservation laws of interest. Starting with
conservation of mass, where $a = \rho$ and
$\mathbf{f} = \mathbf{f}^{\text{mass}} \equiv \rho\mathbf{u}$:

!equation
\int\limits_{\Gamma_{\text{wall}}} \mathbf{f}\cdot\mathbf{n} \,d\Gamma
    = \int\limits_{\Gamma_{\text{wall}}} \rho\mathbf{u}\cdot\mathbf{n} \,d\Gamma
    = 0 \eqc

owing to the wall boundary condition $\mathbf{u}\cdot\mathbf{n} = 0$.

For conservation of momentum in the $x$-direction, $a = \rho u_x$ and
$\mathbf{f} = \mathbf{f}^{\text{mom},x} \equiv \rho u_x\mathbf{u} + p \mathbf{e}_x$:

!equation
\int\limits_{\Gamma_{\text{wall}}} \mathbf{f}\cdot\mathbf{n} \,d\Gamma
    = \int\limits_{\Gamma_{\text{wall}}}
      \pr{\rho u_x\mathbf{u} + p \mathbf{e}_x}\cdot\mathbf{n} \,d\Gamma
    = \int\limits_{\Gamma_{\text{wall}}} p n_x \,d\Gamma \eqp

Then, assuming the pressure to be constant along the surface, equal to the
average pressure in the volume,

!equation
\int\limits_{\Gamma_{\text{wall}}} p n_x \,d\Gamma
    = \bar{p}\int\limits_{\Gamma_{\text{wall}}} n_x \,d\Gamma \eqp


Using the Gauss divergence theorem,

!equation
\int\limits_\Gamma \mathbf{n} \,d\Gamma = \mathbf{0} \eqc

!equation
\int\limits_{\Gamma_{\text{wall}}} \mathbf{n} \,d\Gamma
  + \sum\limits_{i=1}^N \mathbf{n}_{\text{J},i} A_i = \mathbf{0} \eqc

!equation
\int\limits_{\Gamma_{\text{wall}}} \mathbf{n} \,d\Gamma
  = - \sum\limits_{i=1}^N \mathbf{n}_{\text{J},i} A_i \eqp

Therefore,

!equation
\bar{p} \int\limits_{\Gamma_{\text{wall}}} n_x \,d\Gamma
  = \bar{p} \sum\limits_{i=1}^N n_{i,x} A_i \eqp

Conservation of momentum in the $y$ and $z$ directions proceeds similarly.

Finally, conservation of energy has $a = \rho E$
and $\mathbf{f} = \mathbf{f}^{\text{energy}} \equiv \pr{\rho E + p}\mathbf{u}$:

!equation
\int\limits_{\Gamma_{\text{wall}}} \mathbf{f}\cdot\mathbf{n} \,d\Gamma
  = \int\limits_{\Gamma_{\text{wall}}}
    \pr{\rho E + p}\mathbf{u}\cdot\mathbf{n} \,d\Gamma
  = 0 \eqc

again owing to wall boundary conditions.

Putting everything together and denoting the volume-average quantities with
the subscript $\text{J}$ gives

!equation
\ddt{\pr{\rho V}_\text{J}} =
  -\sum\limits_{i=1}^N f_{n,i}^{\text{mass}} A_i \eqc

!equation
\ddt{\pr{\rho u_x V}_\text{J}} =
  -\sum\limits_{i=1}^N f_{n,i}^{\text{mom},x} A_i
  - p_\text{J} \sum\limits_{i=1}^N n_{i,x} A_i \eqc

!equation
\ddt{\pr{\rho u_y V}_\text{J}} =
  -\sum\limits_{i=1}^N f_{n,i}^{\text{mom},y} A_i
  - p_\text{J} \sum\limits_{i=1}^N n_{i,y} A_i \eqc

!equation
\ddt{\pr{\rho u_z V}_\text{J}} =
  -\sum\limits_{i=1}^N f_{n,i}^{\text{mom},z} A_i
  - p_\text{J} \sum\limits_{i=1}^N n_{i,z} A_i \eqc

!equation
\ddt{\pr{\rho E V}_\text{J}} =
  -\sum\limits_{i=1}^N f_{n,i}^{\text{energy}} A_i \eqp

where $f^y_{n,i}$ is defined as

!equation
f^y_{n,i} \equiv \mathbf{f}^y_i \cdot \mathbf{n}_{\text{J},i} \eqp

### Fluxes

This section gives details on the computation of the fluxes $f^y_{n,i}$ referenced
in the previous section. These fluxes are computed using a numerical
flux function,

!equation
\mathcal{F}(\mathbf{U}_\text{L}, \mathbf{U}_\text{R}, \mathbf{n}_{\text{L},\text{R}}, \mathbf{t}_1, \mathbf{t}_2)
  = \left[\begin{array}{c}
  f^\text{mass}_n \\
  f^{\text{mom},n}_n \\
  f^{\text{mom},t_1}_n \\
  f^{\text{mom},t_2}_n \\
  f^\text{energy}_n
  \end{array}\right] \eqc

where the vector $\mathbf{n}_{\text{L},\text{R}}$ is the outward unit vector from
the "L" state to the "R" state, and $\mathbf{t}_1$ and $\mathbf{t}_2$ are arbitrary
unit vectors forming an orthonormal basis with $\mathbf{n}_{\text{L},\text{R}}$.

The "L" state is taken to be the junction, and the "R" state is taken
to be the channel $i$:

!equation
\mathbf{U}_{\text{J},i} \equiv \left[\begin{array}{c}
  \rho_\text{J} \\
  \rho_\text{J} \mathbf{u}_\text{J} \\
  \rho_\text{J} E_\text{J}
\end{array}\right] \eqc

!equation
\mathbf{U}_i \equiv \left[\begin{array}{c}
  \rho_i \\
  \rho_i u_{d,i} \mathbf{d}_i \\
  \rho_i E_i
\end{array}\right] \eqc

!equation
\mathbf{n}_{\text{L},\text{R}} \equiv \mathbf{n}_{\text{J},i} \eqc

!equation
\left[\begin{array}{c}
  f^\text{mass}_{n,i} \\
  f^{\text{mom},n}_{n,i} \\
  f^{\text{mom},t_1}_{n,i} \\
  f^{\text{mom},t_2}_{n,i} \\
  f^\text{energy}_{n,i}
  \end{array}\right]
  = \mathcal{F}(\mathbf{U}_{\text{J},i}, \mathbf{U}_i, \mathbf{n}_{\text{J},i}, \mathbf{t}_{1,i}, \mathbf{t}_{2,i}) \eqc

where $\mathbf{d}_i$ is the local orientation vector for flow channel $i$, equal or opposite to $\mathbf{n}_i$.

The input velocities here are in the global Cartesian basis $\{\mathbf{e}_x, \mathbf{e}_y, \mathbf{e}_z\}$,
whereas the numerical flux function returns momentum components in the
normal basis $\{\mathbf{n}_{\text{L},\text{R}}, \mathbf{t}_1, \mathbf{t}_2\}$,
so one must perform a change of basis afterward:

!equation
f^{\text{mom},x}_{n,i} = (f^{\text{mom},n}_{n,i} \mathbf{n}_{\text{J},i}
  + f^{\text{mom},t_1}_{n,i} \mathbf{t}_{1,i} + f^{\text{mom},t_2}_{n,i} \mathbf{t}_{2,i})
  \cdot \mathbf{e}_x \eqp

!equation
f^{\text{mom},y}_{n,i} = (f^{\text{mom},n}_{n,i} \mathbf{n}_{\text{J},i}
  + f^{\text{mom},t_1}_{n,i} \mathbf{t}_{1,i} + f^{\text{mom},t_2}_{n,i} \mathbf{t}_{2,i})
  \cdot \mathbf{e}_y \eqp

!equation
f^{\text{mom},z}_{n,i} = (f^{\text{mom},n}_{n,i} \mathbf{n}_{\text{J},i}
  + f^{\text{mom},t_1}_{n,i} \mathbf{t}_{1,i} + f^{\text{mom},t_2}_{n,i} \mathbf{t}_{2,i})
  \cdot \mathbf{e}_z \eqp

### Form Losses

!template load file=volume_junction_1phase_formulation_formloss.md.template name=VolumeJunction1Phase

!syntax inputs /Components/VolumeJunction1Phase

!syntax children /Components/VolumeJunction1Phase

!bibtex bibliography
