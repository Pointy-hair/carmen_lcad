/*********************************************************
	 ---   My Module Specific Messages ---

See IPC documentation for more information:
http://www.cs.cmu.edu/~ipc/

*********************************************************/

#include <carmen/grid_mapping.h>
#include <carmen/map_io.h>
#include <carmen/map_interface.h>
#include <carmen/map_server_interface.h>

#ifndef CARMEN_MOVING_OBJECTS_MAP_MESSAGES_H
#define CARMEN_MOVING_OBJECTS_MAP_MESSAGES_H


/********************
 * laser - velodyne *
 ********************/
#ifndef CARMEN_MOVING_OBJECTS_POINT_CLOUDS_MESSAGES_H
#define CARMEN_MOVING_OBJECTS_POINT_CLOUDS_MESSAGES_H

#ifdef __cplusplus
extern "C" {
#endif


/************
 * grid map *
 ************/
typedef struct {
	double *complete_map;
	int size;
	carmen_map_config_t config;
	double timestamp;			/* !!! obligatory !!! */
	char *host;					/* !!! obligatory !!! */
} carmen_moving_objects_map_message;

#define CARMEN_MOVING_OBJECTS_MAP_MESSAGE_NAME "carmen_moving_objects_map_message_name"
#define CARMEN_MOVING_OBJECTS_MAP_MESSAGE_FMT		"{<double:2>, int, {int, int, double, [byte:64], string, double, double}, double, string}"


/*********
 * laser *
 *********/
typedef struct {
	double x;
	double y;
	int label;  // 1: moving object; 0: static object
	int cluster; // is core cluster
	double range;
} cartesian_points;

typedef struct {
	cartesian_points *points;
	int num_points;
	int num_clusters;
} carmen_objects_cartesian_points;

typedef struct {
	double *range;
	carmen_pose_3D_t car_pose;
	carmen_vector_3D_t *points;
	carmen_laser_laser_config_t config;
	carmen_vector_3D_t velocity;
	double pose_time;
	double phi;
	int num_points;
	int num_readings;
} carmen_laser_cartesian_points;


/***************************
 * velodyne - point clouds *
 ***************************/
struct _object_geometry
{
	double length;
	double width;
	double height;
};

typedef struct _object_geometry object_geometry_t;

struct _object_model_features {
	int model_id;
	char *model_name; //short description of the type of model
	object_geometry_t geometry;
	double red, green, blue;
};

typedef struct _object_model_features object_model_features_t;

struct _particle_print {
	carmen_point_t pose;
	double velocity;
	int class_id;
	object_geometry_t geometry;
};

typedef struct _particle_print particle_print_t;

typedef struct {
	int point_size;
	double r, g, b;
	double linear_velocity;
	double orientation;
	double length;
	double width;
	double height;
	int geometric_model;
	object_model_features_t model_features;
	int	num_associated;
	carmen_vector_3D_t object_pose;
	carmen_vector_3D_t *points;
	// particle_print_t *particulas; // para a visualização das partículas
} t_point_cloud_struct;

typedef struct {
	int num_point_clouds;
	t_point_cloud_struct *point_clouds;
	double timestamp; 		/* !!! obrigatory !!! */
	char *host; 			/* !!! obrigatory !!! */
} carmen_moving_objects_point_clouds_message;

/* The message's name, will be used for message registration in IPC Central module */
#define CARMEN_MOVING_OBJECTS_POINT_CLOUDS_MESSAGE_NAME 	"carmen_moving_objects_point_clouds_message_name"

/* The message's format, will be used for message data marshalling (serialization) for network socket transport. */
/* fixme                                                                                                                                       model features...............................................*/
#define CARMEN_MOVING_OBJECTS_POINT_CLOUDS_MESSAGE_FMT		"{int, <{int, double, double, double, double, double, double, double, double, int, {int, string, {double, double, double}, double, double, double}, int, {double, double, double}, <{double, double, double}:1>}:1>, double, string}"
//#define CARMEN_MOVING_OBJECTS_POINT_CLOUDS_MESSAGE_FMT		"{int, <{int, double, double, double, double, double, double, double, double, int, {int, string, {double, double, double}, double, double, double}, int, {double, double, double}, <{double, double, double}:1>,<{{double, double, double}, double, int, {double, double, double} }:1>}:1>, double, string}"


struct _moving_objects_tracking
{
	carmen_pose_3D_t moving_objects_pose;
	double linear_velocity;
	double length;
	double width;
	double height;
	int geometric_model;
	object_model_features_t model_features;
	int	num_associated;
	double timestamp;
	//particle_print_t *particulas; // para a visualização das partículas
};

typedef struct _moving_objects_tracking moving_objects_tracking_t;

#ifdef __cplusplus
}
#endif

#endif
#endif

// @}
