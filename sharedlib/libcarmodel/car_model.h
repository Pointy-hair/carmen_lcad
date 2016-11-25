#ifndef CAR_NEURAL_MODEL_H
#define CAR_NEURAL_MODEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <carmen/carmen.h>
#include <fann.h>
#include <fann_train.h>
#include <fann_data.h>
#include <floatfann.h>
#include <control.h>

#define NUM_VELOCITY_ANN_INPUTS 360
#define NUM_STEERING_ANN_INPUTS 80


carmen_ackerman_traj_point_t
carmen_libcarmodel_recalc_pos_ackerman(carmen_ackerman_traj_point_t robot_state, double target_v, double target_phi,
		double full_time_interval, double *distance_traveled, double delta_t, carmen_robot_ackerman_config_t robot_config);


void carmen_libcarneuralmodel_init_steering_ann_input(fann_type *input);

void carmen_libcarneuralmodel_build_steering_ann_input(fann_type *input, double s, double cc);

double
carmen_libcarneuralmodel_compute_new_phi_from_effort(double steering_effort, double atan_current_curvature, fann_type *steering_ann_input, struct fann *steering_ann,
														double v, double understeer_coeficient, double distance_between_front_and_rear_axles,
														double max_phi);

void carmen_libcarneuralmodel_init_velocity_ann_input(fann_type *input);

void carmen_libcarneuralmodel_build_velocity_ann_input(fann_type *input, double t, double b, double cv);

//void carmen_libcarneuralmodel_recalc_pos(carmen_simulator_ackerman_config_t *simulator_config);


#ifdef __cplusplus
}
#endif

#endif /* CAR_NEURAL_MODEL_H */
