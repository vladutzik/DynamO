/*    DYNAMO:- Event driven molecular dynamics simulator 
 *    http://www.marcusbannerman.co.uk/dynamo
 *    Copyright (C) 2009  Marcus N Campbell Bannerman <m.bannerman@gmail.com>
 *
 *    This program is free software: you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    version 3 as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <cmath>

namespace magnet {
  namespace GL {    
    struct viewPort
    {
      inline viewPort(Vector position = Vector(0,0,0), 
		      Vector lookAtPoint = Vector(0,0,1),
		      GLfloat fovY = 45.0f,
		      GLfloat zNearDist = 0.001f, GLfloat zFarDist = 100.0f,
		      Vector up = Vector(0,1,0)
		      ):
	_rotatex(180),
	_rotatey(0),
	_position(position),
	_fovY(fovY),
	_aspectRatio(1),
	_zNearDist(zNearDist),
	_zFarDist(zFarDist)
      {
	_rotatex = 180 + std::acos((lookAtPoint - Vector(0,1,0) * (lookAtPoint | Vector(0,1,0))) | Vector(0,0,1));
	_rotatey = 0   + std::acos((lookAtPoint - Vector(0,0,1) * (lookAtPoint | Vector(0,1,0))) | Vector(0,1,0));
      }
      
      inline void CameraSetup(float forward = 0, float sideways = 0, float vertical = 0)
      {
	//Forward/Backward movement
	_position[2] -= forward * std::cos(_rotatey * (M_PI/ 180)) 
	  * std::sin(_rotatex  * (M_PI/ 180) + M_PI * 0.5);  
	_position[0] -= forward * std::cos(_rotatey * (M_PI/ 180)) 
	  * std::cos(_rotatex  * (M_PI/ 180) + M_PI * 0.5);
	_position[1] += -forward * std::sin(_rotatey * (M_PI/ 180));
	
	//Strafe movement
	_position[2] += sideways * std::sin(_rotatex * (M_PI/ 180));
	_position[0] += sideways * std::cos(_rotatex * (M_PI/ 180));
	
	//Vertical movement
	_position[1] += vertical;
	
	glLoadIdentity();
	//gluLookAt(-viewscale, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
	glRotatef(_rotatey, 1.0, 0.0, 0.0);
	glRotatef(_rotatex, 0.0, 1.0, 0.0);
	glTranslatef(-_position[0], -_position[1], -_position[2]);
	
	//store the matricies for shadow calculations
	glGetFloatv(GL_MODELVIEW_MATRIX, _viewMatrix);
	glGetFloatv(GL_PROJECTION_MATRIX, _projectionMatrix);
	
	Matrix viewTransform = Rodrigues(Vector(0,-_rotatex * M_PI/180,0)) 
	  * Rodrigues(Vector(-_rotatey * M_PI/180.0,0,0));
	
	_cameraDirection =  viewTransform * Vector(0,0,-1);
	_cameraUp = viewTransform * Vector(0,1,0);
      }
      
      float _rotatex;
      float _rotatey;
      Vector _position;
      
      GLdouble _fovY;
      GLdouble _aspectRatio;
      GLdouble _zNearDist;
      GLdouble _zFarDist;
      
      Vector _cameraDirection, _cameraUp;
      
      MATRIX4X4 _projectionMatrix;
      MATRIX4X4 _viewMatrix;
    };
  }
}
