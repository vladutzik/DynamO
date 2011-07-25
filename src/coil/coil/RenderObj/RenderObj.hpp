/*  dynamo:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
    Copyright (C) 2011  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

    This program is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 3 as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include <magnet/GL/context.hpp>
#include <magnet/thread/refPtr.hpp>
#include <magnet/thread/taskQueue.hpp>
#include <magnet/GL/camera.hpp>
#include <magnet/GL/FBO.hpp>

namespace Gtk {
  class ScrolledWindow;
}

namespace magnet {
  namespace GL {
    class Camera;
  }
}

namespace coil {
  class RenderObj
  {
  public:
    RenderObj(std::string name):
      _name(name),
      _RenderMode(TRIANGLES),
      _renderNormals(false),
      _visible(true)
    {}
  
    ~RenderObj() {}
  
    virtual void init(const magnet::thread::RefPtr<magnet::thread::TaskQueue>& systemQueue) 
    { _systemQueue = systemQueue; }
    virtual void initGTK() {}
    virtual void deinit() {}
    virtual void clTick(const magnet::GL::Camera& cam) = 0;
    virtual void glRender(magnet::GL::FBO& fbo, const magnet::GL::Camera& cam) = 0;
    virtual void interfaceRender(const magnet::GL::Camera& camera) {}
    virtual void initPicking(cl_uint& offset) {}
    virtual void pickingRender(magnet::GL::FBO& fbo, const magnet::GL::Camera& cam) {}
    virtual void finishPicking(cl_uint& offset, const cl_uint val) {}
    virtual void showControls(Gtk::ScrolledWindow* win) {}

    enum RenderModeType { POINTS, LINES, TRIANGLES };

    virtual void setRenderMode(RenderModeType rm) { _RenderMode = rm; }
    inline void setDisplayNormals(bool val) { _renderNormals = val; }
    inline void setVisible(bool val) { _visible = val; }
    inline bool isVisible() const { return _visible; }
    inline const std::string& getName() const { return _name; }
    magnet::thread::RefPtr<magnet::thread::TaskQueue> getQueue() { return _systemQueue; }

  protected:
    std::string _name;

    RenderModeType _RenderMode;
    bool _renderNormals;
    bool _visible;
    magnet::thread::RefPtr<magnet::thread::TaskQueue> _systemQueue;
  };
}
