/*!@file Devices/IMU_IMU_SFE_Atomic.H class
 for interfacing with the Spark Fun Electronics Atomic IMU */

// //////////////////////////////////////////////////////////////////// //
// The iLab Neuromorphic Vision C++ Toolkit - Copyright (C) 2000-2003   //
// by the University of Southern California (USC) and the iLab at USC.  //
// See http://iLab.usc.edu for information about this project.          //
// //////////////////////////////////////////////////////////////////// //
// Major portions of the iLab Neuromorphic Vision Toolkit are protected //
// under the U.S. patent ``Computation of Intrinsic Perceptual Saliency //
// in Visual Environments, and Applications'' by Christof Koch and      //
// Laurent Itti, California Institute of Technology, 2001 (patent       //
// pending; application number 09/912,225 filed July 23, 2001; see      //
// http://pair.uspto.gov/cgi-bin/final/home.pl for current status).     //
// //////////////////////////////////////////////////////////////////// //
// This file is part of the iLab Neuromorphic Vision C++ Toolkit.       //
//                                                                      //
// The iLab Neuromorphic Vision C++ Toolkit is free software; you can   //
// redistribute it and/or modify it under the terms of the GNU General  //
// Public License as published by the Free Software Foundation; either  //
// version 2 of the License, or (at your option) any later version.     //
//                                                                      //
// The iLab Neuromorphic Vision C++ Toolkit is distributed in the hope  //
// that it will be useful, but WITHOUT ANY WARRANTY; without even the   //
// implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      //
// PURPOSE.  See the GNU General Public License for more details.       //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with the iLab Neuromorphic Vision C++ Toolkit; if not, write   //
// to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,   //
// Boston, MA 02111-1307 USA.                                           //
// //////////////////////////////////////////////////////////////////// //
//


#ifndef IMU_SFE_ATOMIC_H
#define IMU_SFE_ATOMIC_H

#include "Component/OptionManager.H"
#include "Devices/Serial.H"

class IMU_SFE_Atomic : public ModelComponent
{
  public:
    IMU_SFE_Atomic(OptionManager& mgr,
        const std::string& descrName = "IMU_SFE_Atomic",
        const std::string& tagName   = "IMU_SFE_Atomic");

    struct IMUData
    {
      float accelX;
      float accelY;
      float accelZ;
      float pitch;
      float roll;
      float yaw;
      float count;
    };

    IMUData readIMUData();

    void start2();

  private:
    nub::ref<Serial> itsSerial;
};

#endif
