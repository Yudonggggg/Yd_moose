//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

// MOOSE includes
#include "Cylinder.h"

/**
 * Component which depends on another component for a setup-subtask
 */
class CylinderWithDependencies : public Cylinder
{
public:
  /**
   * Class constructor
   */
  static InputParameters validParams();

  CylinderWithDependencies(const InputParameters & params);

protected:
  virtual void setupComponent() override;
  // NOTE: maybe we could rename this actOnDependentTasks?
  //       or make the logic on base act() resilient to dependent tasks
  virtual void actOnAdditionalTasks() override;
};
