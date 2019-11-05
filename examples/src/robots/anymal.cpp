//
// Created by Jemin Hwangbo on 2/28/19.
// MIT License
//
// Copyright (c) 2019-2019 Robotic Systems Lab, ETH Zurich
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


#include <raisim/OgreVis.hpp>
#include "anymal_imgui_render_callback.hpp"
#include "raisimKeyboardCallback.hpp"
#include "helper.hpp"

namespace raisim {
namespace anymal_gui {
std::vector<std::vector<float>> jointSpeed, jointTorque;
std::vector<float> time;
}
}

void setupCallback() {
  auto vis = raisim::OgreVis::get();

  /// light
  vis->getLight()->setDiffuseColour(1, 1, 1);
  vis->getLight()->setCastShadows(true);
  Ogre::Vector3 lightdir(-3,-3,-0.5);
  lightdir.normalise();
  vis->getLightNode()->setDirection({lightdir});
  vis->setCameraSpeed(300);

  /// load  textures
  vis->addResourceDirectory(vis->getResourceDir() + "/material/checkerboard");
  vis->loadMaterialFile("checkerboard.material");

  /// shdow setting
  vis->getSceneManager()->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_ADDITIVE);
  vis->getSceneManager()->setShadowTextureSettings(2048, 3);

  /// scale related settings!! Please adapt it depending on your map size
  // beyond this distance, shadow disappears
  vis->getSceneManager()->setShadowFarDistance(10);
  // size of contact points and contact forces
  vis->setContactVisObjectSize(0.03, 0.6);
  // speed of camera motion in freelook mode
  vis->getCameraMan()->setTopSpeed(5);

  /// skybox
  Ogre::Quaternion quat;
  quat.FromAngleAxis(Ogre::Radian(M_PI_2), {1., 0, 0});
  vis->getSceneManager()->setSkyBox(true,
                                    "Examples/StormySkyBox",
                                    500,
                                    true,
                                    quat);
}

int main(int argc, char **argv) {
  /// create raisim world
  raisim::World world;
  world.setTimeStep(0.0025);

  auto vis = raisim::OgreVis::get();

  /// gui
  raisim::anymal_gui::init();

  /// these method must be called before initApp
  vis->setWorld(&world);
  vis->setWindowSize(1800, 1200);
  vis->setImguiSetupCallback(raisim::anymal_gui::imguiSetupCallback);
  vis->setImguiRenderCallback(raisim::anymal_gui::anymalImguiRenderCallBack);
  vis->setKeyboardCallback(raisimKeyboardCallback);
  vis->setSetUpCallback(setupCallback);
  vis->setAntiAliasing(2);

  /// starts visualizer thread
  vis->initApp();

  /// create raisim objects
  auto ground = world.addGround();
  ground->setName("checkerboard");

  raisim::ArticulatedSystem* anymal;

  /// create visualizer objects
  vis->createGraphicalObject(ground, 20, "floor", "checkerboard_green");

  /// ANYmal joint PD controller
  Eigen::VectorXd jointNominalConfig(19), jointVelocityTarget(18);
  Eigen::VectorXd jointState(18), jointForce(18), jointPgain(18), jointDgain(18);
  Eigen::VectorXd jointTorque(12), jointSpeed(12);


  jointPgain.setZero();
  jointDgain.setZero();
  jointVelocityTarget.setZero();
  jointPgain.tail(12).setConstant(200.0);
  jointDgain.tail(12).setConstant(10.0);

  anymal = world.addArticulatedSystem(raisim::loadResource("anymal/anymal.urdf"));
  auto anymal_graphics = vis->createGraphicalObject(anymal, "ANYmal"); // this is the name assigned for raisimOgre. It is displayed using this name
  anymal->setGeneralizedCoordinate({0, 0, 0.54, 1.0, 0.0, 0.0, 0.0, 0.03, 0.4,
                                    -0.8, -0.03, 0.4, -0.8, 0.03, -0.4, 0.8, -0.03, -0.4, 0.8});
  anymal->setGeneralizedForce(Eigen::VectorXd::Zero(anymal->getDOF()));
  anymal->setControlMode(raisim::ControlMode::PD_PLUS_FEEDFORWARD_TORQUE);
  anymal->setPdGains(jointPgain, jointDgain);
  anymal->setName("anymal"); // this is the name assigned for raisim. Not used in this example

  std::default_random_engine generator;
  std::normal_distribution<double> distribution(0.0, 0.2);
  std::srand(std::time(nullptr));
  double time=0.;

  // lambda function for the controller
  auto controller = [anymal, &generator, &distribution, &jointTorque, &jointSpeed, &time, &world]() {
    static size_t controlDecimation = 0;
    time += world.getTimeStep();

    if(controlDecimation++ % 2500 == 0) {
      anymal->setGeneralizedCoordinate({0, 0, 0.54, 1.0, 0.0, 0.0, 0.0, 0.03, 0.4,
                                        -0.8, -0.03, 0.4, -0.8, 0.03, -0.4, 0.8, -0.03, -0.4, 0.8});
      raisim::anymal_gui::clear();
      time = 0.;
    }

    if(controlDecimation % 50 != 0)
      return;

    /// ANYmal joint PD controller
    Eigen::VectorXd jointNominalConfig(19), jointVelocityTarget(18);
    jointVelocityTarget.setZero();
    jointNominalConfig << 0, 0, 0, 0, 0, 0, 0, 0.03, 0.3, -.6, -0.03, 0.3, -.6, 0.03, -0.3, .6, -0.03, -0.3, .6;

    for (size_t k = 0; k < anymal->getGeneralizedCoordinateDim(); k++)
      jointNominalConfig(k) += distribution(generator);

    anymal->setPdTarget(jointNominalConfig, jointVelocityTarget);

    jointTorque = anymal->getGeneralizedForce().e().tail(12);
    jointSpeed = anymal->getGeneralizedVelocity().e().tail(12);

    raisim::anymal_gui::push_back(time, jointSpeed, jointTorque);
  };

  vis->setControlCallback(controller);

  /// set camera
  vis->select(anymal_graphics->at(0), false);
  vis->getCameraMan()->setYawPitchDist(Ogre::Radian(0), -Ogre::Radian(M_PI_4), 2);

  /// run the app
  vis->run();

  /// terminate
  vis->closeApp();

  return 0;
}