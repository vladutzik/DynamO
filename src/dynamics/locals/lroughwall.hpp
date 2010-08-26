/*  DYNAMO:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
    Copyright (C) 2010  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

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

#ifndef LRoughWall_HPP
#define LRoughWall_HPP

#include "local.hpp"

class LRoughWall: public Local
{
public:
  LRoughWall(const XMLNode&, DYNAMO::SimData*);
  LRoughWall(DYNAMO::SimData*, Iflt, Iflt, Iflt, Vector , Vector , 
	 std::string, CRange*, bool nrender = true);

  virtual ~LRoughWall() {}

  virtual Local* Clone() const { return new LRoughWall(*this); };

  virtual LocalEvent getEvent(const Particle&) const;

  virtual void runEvent(const Particle&, const LocalEvent&) const;
  
  virtual bool isInCell(const Vector &, const Vector &) const;

  virtual void initialise(size_t);

  virtual void operator<<(const XMLNode&);

  virtual void write_povray_info(std::ostream&) const;

  virtual void checkOverlaps(const Particle&) const;

protected:
  virtual void outputXML(xml::XmlStream&) const;

  Vector  vNorm;
  Vector  vPosition;
  Iflt e;
  Iflt et;
  Iflt r;
  bool render;
};

#endif