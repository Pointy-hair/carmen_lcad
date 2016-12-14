/*********************************************************
 *
 * This source code is part of the Carnegie Mellon Robot
 * Navigation Toolkit (CARMEN)
 *
 * CARMEN Copyright (c) 2002 Michael Montemerlo, Nicholas
 * Roy, Sebastian Thrun, Dirk Haehnel, Cyrill Stachniss,
 * and Jared Glover
 *
 * CARMEN is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public 
 * License as published by the Free Software Foundation; 
 * either version 2 of the License, or (at your option)
 * any later version.
 *
 * CARMEN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more 
 * details.
 *
 * You should have received a copy of the GNU General 
 * Public License along with CARMEN; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, 
 * Suite 330, Boston, MA  02111-1307 USA
 *
 ********************************************************/

#ifndef FORD_ESCAPE_HYBRID_H
#define FORD_ESCAPE_HYBRID_H

#include "ford_escape_hybrid_messages.h"
#include <carmen/localize_ackerman_motion.h>
#include <carmen/fused_odometry_messages.h>


#define NUM_MOTION_COMMANDS_VECTORS	5
#define	NUM_MOTION_COMMANDS_PER_VECTOR	200


#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
	double filtered_v;
	double filtered_phi;
	double filtered_pitch;

	double width;
	double length;
	double distance_between_front_and_rear_axles;
//	double understeer_coeficient;
	double understeer_coeficient2; // Sera que isso soh eh preciso ao ler da IARA? Se sim, o paramentro acima deve ser zero em todos os modulos
	double max_phi;
	
	carmen_ackerman_motion_command_p current_motion_command_vector;
	int nun_motion_commands;
	double time_of_last_command;
	
	double XGV_v_and_phi_timestamp;

	unsigned int use_mpc;
	unsigned int use_rlpid;
} ford_escape_hybrid_config_t, *ford_escape_hybrid_config_p;

#ifdef __cplusplus
}
#endif

#endif
