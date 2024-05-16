//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

// MOOSE includes
#include "CylinderWithDependencies.h"

registerMooseAction("MooseApp", CylinderWithDependencies, "add_mesh_generator");
registerMooseAction("MooseApp", CylinderWithDependencies, "init_physics");
registerMooseAction("MooseApp", CylinderWithDependencies, "setup_component");

InputParameters
CylinderWithDependencies::validParams()
{
  InputParameters params = Cylinder::validParams();
  params.addClassDescription("Cylindrical component to test the addition of dependencies.");

  params.addParam<std::vector<ComponentName>>(
      "setup_dependencies", {}, "Components this component depends on for the setup task");
  return params;
}

CylinderWithDependencies::CylinderWithDependencies(const InputParameters & params)
  : Cylinder(params)
{
  // Dynamically add a sub-task for setup_component
  // Note that we do not know whether this component will be depended on, so we must always register
  // our own task?
  // TODO: create an API for automatically creating the additional tasks for tasks with
  // inter-component dependencies
  // TODO: should we add the task? Seems so since we want to act on it
  const auto new_task_name = "setup_component_" + name();
  _awh.lateAddAction(new_task_name, this);

  // Make it depend on 'setup_component' so it happens near that task
  // If all the dependencies were declared correctly, the fact that it's after should not be an
  // issue
  _app.syntax().addDependency(new_task_name, "setup_component");

  // Add the dependencies on the other components
  for (const auto & other_comp_name : getParam<std::vector<ComponentName>>("setup_dependencies"))
    _app.syntax().addDependency(new_task_name, "setup_component_" + other_comp_name);
}

void
CylinderWithDependencies::setupComponent()
{
  // TODO: have this work be transferred to the named task
  _console << "Setting up component " << name() << " on task " << _current_task << std::endl;
}

void
CylinderWithDependencies::actOnAdditionalTasks()
{
  std::cout << "Additional ?" << _current_task << std::endl;
  if (_current_task == "setup_component_" + name())
    _console << "Setting up component " << name() << " on task " << _current_task << std::endl;
}
