/*
 * virtual_scan.cpp
 *
 *  Created on: Jul 17, 2017
 *	  Author: claudine
 */
#include <carmen/carmen.h>
#include <algorithm>
#include <string.h>
#include <cmath>
#include <carmen/moving_objects_messages.h>
#include <carmen/velodyne_interface.h>
#include <dbscan.h>
#include <vector>
#include "virtual_scan.h"

extern carmen_localize_ackerman_map_t localize_map;
extern double x_origin;
extern double y_origin;
extern double map_resolution;

#define SMALL_NUM   0.00000001 // anything that avoids division overflow

#define dot(u,v)   ((u).x * (v).x + (u).y * (v).y)
#define perp(u,v)  ((u).x * (v).y - (u).y * (v).x)  // perp product  (2D)

typedef struct {
    carmen_position_t P0;
    carmen_position_t P1;
} line_segment_t;


extern int g_zi;
extern virtual_scan_extended_t *g_virtual_scan_extended[NUMBER_OF_FRAMES_T];

virtual_scan_track_set_t *best_track_set = NULL;


void
print_track_set(virtual_scan_track_set_t *track_set, virtual_scan_neighborhood_graph_t *neighborhood_graph, int num_proposal)
{
	FILE *track_sets = fopen("track_sets.txt", "a");

	if (track_set == NULL)
	{
		fprintf(track_sets, "\nGraph Id %d, Num Proposal %d - track_set == NULL\n", neighborhood_graph->graph_id, num_proposal);
		fclose(track_sets);

		return;
	}

	double prob = probability_of_track_set_given_measurements(track_set, false);

	fprintf(track_sets, "\nGraph Id %d Num Proposal %d - Num tracks = %d, prob = %lf\n", neighborhood_graph->graph_id, num_proposal, track_set->size, prob);
	for (int i = 0; i < neighborhood_graph->size; i++)
		fprintf(track_sets, "v[%d] %d, ", i, track_set->vertex_selected[i]);
	fprintf(track_sets, "\n");

	for (int i = 0; i < track_set->size; i++)
	{
		fprintf(track_sets, "track %d: ", i);
		for (int j = 0; j < track_set->tracks[i]->size; j++)
			fprintf(track_sets, "h %d - %c, index %d, zi %d;  ", j, track_set->tracks[i]->box_model_hypothesis[j].hypothesis.c,
					track_set->tracks[i]->box_model_hypothesis[j].index,
					track_set->tracks[i]->box_model_hypothesis[j].zi);
		fprintf(track_sets, "\n");
	}

	fprintf(track_sets, "\n");
	fclose(track_sets);
}


int
compare_angles(const void *a, const void *b)
{
	carmen_point_t *arg1 = (carmen_point_t *) a;
	carmen_point_t *arg2 = (carmen_point_t *) b;

	double delta_theta = carmen_normalize_theta(arg1->theta - arg2->theta);
	if (delta_theta < 0.0)
		return (-1);
	if (delta_theta > 0.0)
		return (1);

	return (0);
}


virtual_scan_extended_t *
sort_virtual_scan(carmen_mapper_virtual_scan_message *virtual_scan) // Verify if rays are unordered according to theta
{
	virtual_scan_extended_t *extended_virtual_scan = (virtual_scan_extended_t *) malloc(sizeof(virtual_scan_extended_t));
	extended_virtual_scan->points = (carmen_point_t *) malloc(virtual_scan->num_points * sizeof(carmen_point_t));
	extended_virtual_scan->num_points = virtual_scan->num_points;

	carmen_pose_3D_t world_pose = {{virtual_scan->globalpos.x, virtual_scan->globalpos.y, 0.0}, {0.0, 0.0, virtual_scan->globalpos.theta}};
	world_pose = get_world_pose_with_velodyne_offset(world_pose);
	extended_virtual_scan->velodyne_pos = {world_pose.position.x, world_pose.position.y, world_pose.orientation.yaw};

	extended_virtual_scan->timestamp = virtual_scan->timestamp;
	for (int i = 0; i < virtual_scan->num_points; i++)
	{
		extended_virtual_scan->points[i].x = virtual_scan->points[i].x;
		extended_virtual_scan->points[i].y = virtual_scan->points[i].y;
		double theta = atan2(virtual_scan->points[i].y - extended_virtual_scan->velodyne_pos.y, virtual_scan->points[i].x - extended_virtual_scan->velodyne_pos.x);
		theta = carmen_normalize_theta(theta - extended_virtual_scan->velodyne_pos.theta);
		extended_virtual_scan->points[i].theta = theta;
	}
	qsort((void *) (extended_virtual_scan->points), (size_t) extended_virtual_scan->num_points, sizeof(carmen_point_t), compare_angles);

	return (extended_virtual_scan);
}


void
sort_segment_points_by_angle(virtual_scan_segment_t *segment, carmen_point_t velodyne_pos)
{
	for (int i = 0; i < segment->num_points; i++)
	{
		double theta = atan2(segment->points[i].y - velodyne_pos.y, segment->points[i].x - velodyne_pos.x);
		theta = carmen_normalize_theta(theta + velodyne_pos.theta);
		segment->points[i].theta = theta;
	}
	qsort((void *) (segment->points), (size_t) segment->num_points, sizeof(carmen_point_t), compare_angles);
}


virtual_scan_extended_t *
filter_virtual_scan(virtual_scan_extended_t *virtual_scan_extended)
{
	virtual_scan_extended_t *virtual_scan_extended_filtered;
	virtual_scan_extended_filtered = (virtual_scan_extended_t *) calloc(1, sizeof(virtual_scan_extended_t));

	int num_points = 0;
	for (int i = 0; i < virtual_scan_extended->num_points; i++)
	{
		if (DIST2D(virtual_scan_extended->velodyne_pos, virtual_scan_extended->points[i]) < 30.0)
		{
			int x_index_map = (int) round((virtual_scan_extended->points[i].x - x_origin) / map_resolution);
			int y_index_map = (int) round((virtual_scan_extended->points[i].y - y_origin) / map_resolution);
			if ((x_index_map > 0) && (x_index_map < localize_map.config.x_size) &&
				(y_index_map > 0) && (y_index_map < localize_map.config.y_size))
			{
//				if (localize_map.prob[x_index_map][y_index_map] < PROB_THRESHOLD)
				{
					virtual_scan_extended_filtered->points = (carmen_point_t *) realloc(virtual_scan_extended_filtered->points,
										sizeof(carmen_point_t) * (num_points + 1));
					virtual_scan_extended_filtered->points[num_points] = virtual_scan_extended->points[i];
					num_points++;
				}
			}
			else // Inclui (nao filtra) pontos fora do mapa pois o mapa pode estar simplesmente atrasado.
			{
				virtual_scan_extended_filtered->points = (carmen_point_t *) realloc(virtual_scan_extended_filtered->points,
									sizeof(carmen_point_t) * (num_points + 1));
				virtual_scan_extended_filtered->points[num_points] = virtual_scan_extended->points[i];
				num_points++;
			}
		}
	}
	virtual_scan_extended_filtered->num_points = num_points;
	virtual_scan_extended_filtered->velodyne_pos = virtual_scan_extended->velodyne_pos;
	virtual_scan_extended_filtered->timestamp = virtual_scan_extended->timestamp;

//	fprintf(stdout,"virtual_scan_extended->num_points = %d\n", virtual_scan_extended->num_points);
//	fprintf(stdout,"virtual_scan_extended_filtered->num_points = %d\n\n", virtual_scan_extended_filtered->num_points);
//	fflush(stdout);

	return (virtual_scan_extended_filtered);
}


dbscan::Cluster
generate_cluster_with_all_points(carmen_point_t *points, int size)
{
	dbscan::Cluster cluster;
	for (int i = 0; i < size; i++)
	{
		carmen_point_t point;
		point.x = points[i].x;
		point.y = points[i].y;
		cluster.push_back(point);
	}
	return (cluster);
}


virtual_scan_segment_classes_t *
segment_virtual_scan(virtual_scan_extended_t *extended_virtual_scan)
{
	virtual_scan_segment_classes_t *virtual_scan_segments = (virtual_scan_segment_classes_t *) malloc(sizeof(virtual_scan_segment_classes_t));
	virtual_scan_segments->segment = NULL;
	virtual_scan_segments->num_segments = 0;
	virtual_scan_segments->timestamp = extended_virtual_scan->timestamp;

	dbscan::Cluster single_cluster = generate_cluster_with_all_points(extended_virtual_scan->points, extended_virtual_scan->num_points);
	dbscan::Clusters clusters = dbscan::dbscan(8.0 * PEDESTRIAN_RADIUS * PEDESTRIAN_RADIUS, MINIMUN_CLUSTER_SIZE, single_cluster);

	int p_in_c = 0;
	if (clusters.size() > 0)
	{
		virtual_scan_segments->segment = (virtual_scan_segment_t *) malloc(sizeof(virtual_scan_segment_t) * clusters.size());
		virtual_scan_segments->num_segments = clusters.size();

		for (unsigned int segment_id = 0; segment_id < clusters.size(); segment_id++)
		{
			dbscan::Cluster cluster = clusters[segment_id];
			p_in_c += cluster.size();
			virtual_scan_segments->segment[segment_id].points = (carmen_point_t *) malloc(sizeof(carmen_point_t) * cluster.size());
			virtual_scan_segments->segment[segment_id].num_points = cluster.size();
			for (unsigned int i = 0; i < cluster.size(); i++)
				virtual_scan_segments->segment[segment_id].points[i] = cluster[i];

			sort_segment_points_by_angle(&(virtual_scan_segments->segment[segment_id]), extended_virtual_scan->velodyne_pos);
		}
	}

//	printf("points %d, clusters %ld, points in clusters %d\n", extended_virtual_scan->num_points, clusters.size(), p_in_c);
	return (virtual_scan_segments);
}


static double
dist2(carmen_point_t v, carmen_point_t w)
{
	return (carmen_square(v.x - w.x) + carmen_square(v.y - w.y));
}


static double
dist2(carmen_position_t v, carmen_position_t w)
{
	return (carmen_square(v.x - w.x) + carmen_square(v.y - w.y));
}


carmen_point_t
distance_from_point_to_line_segment_vw(int *point_in_trajectory_is, carmen_point_t v, carmen_point_t w, carmen_point_t p)
{
	// Return minimum distance between line segment vw and points p
	// http://stackoverflow.com/questions/849211/shortest-distance-between-a-points-and-a-line-segment
	double l2, t;

	l2 = dist2(v, w); // i.e. |w-v|^2 // NAO TROQUE POR carmen_ackerman_traj_distance2(&v, &w) pois nao sei se ee a mesma coisa.
	if (sqrt(l2) < PEDESTRIAN_RADIUS)	  // v ~== w case // @@@ Alberto: Checar isso
	{
		*point_in_trajectory_is = SEGMENT_TOO_SHORT;
		return (v);
	}

	// Consider the line extending the segment, parameterized as v + t (w - v).
	// We find the projection of point p onto the line.
	// It falls where t = [(p-v) . (w-v)] / |w-v|^2
	t = ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2;

	if (t < 0.0) 	// p beyond the v end of the segment
	{
		*point_in_trajectory_is = POINT_BEFORE_SEGMENT;
		return (v);
	}
	if (t > 1.0)	// p beyond the w end of the segment
	{
		*point_in_trajectory_is = POINT_AFTER_SEGMENT;
		return (w);
	}

	// Projection falls on the segment
	p = v; // Copy other elements, like theta, etc.
	p.x = v.x + t * (w.x - v.x);
	p.y = v.y + t * (w.y - v.y);
	*point_in_trajectory_is = POINT_WITHIN_SEGMENT;

	return (p);
}


double
distance_from_point_to_line_segment_vw(carmen_position_t v, carmen_position_t w, carmen_point_t p)
{
	// Return minimum distance between line segment vw and points p
	// http://stackoverflow.com/questions/849211/shortest-distance-between-a-points-and-a-line-segment
	double l2, t;

	l2 = dist2(v, w); // i.e. |w-v|^2 // NAO TROQUE POR carmen_ackerman_traj_distance2(&v, &w) pois nao sei se ee a mesma coisa.

	// Consider the line extending the segment, parameterized as v + t (w - v).
	// We find the projection of point p onto the line.
	// It falls where t = [(p-v) . (w-v)] / |w-v|^2
	t = ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2;

//	if (t < 0.0) 	// p beyond the v end of the segment
//	{
//	}
//	if (t > 1.0)	// p beyond the w end of the segment
//	{
//	}

	// Projection falls on the segment
	double x = v.x + t * (w.x - v.x);
	double y = v.y + t * (w.y - v.y);

	double dx = p.x - x;
	double dy = p.y - y;

	return (sqrt(dx * dx + dy * dy));
}


carmen_point_t
compute_segment_centroid(virtual_scan_segment_t virtual_scan_segment)
{
	carmen_point_t centroid = {0.0, 0.0, 0.0};

	for (int i = 0; i < virtual_scan_segment.num_points; i++)
	{
		centroid.x += virtual_scan_segment.points[i].x;
		centroid.y += virtual_scan_segment.points[i].y;
	}
	centroid.x /= (double) virtual_scan_segment.num_points;
	centroid.y /= (double) virtual_scan_segment.num_points;

	return (centroid);
}


void
set_segment_features(virtual_scan_segment_classes_t *virtual_scan_segment_classes, int segment, carmen_point_t first_point,
		carmen_point_t last_point, carmen_point_t farthest_point, double width, double length,
		int segment_class, carmen_point_t centroid)
{
	virtual_scan_segment_classes->segment_features[segment].centroid = centroid;

	virtual_scan_segment_classes->segment_features[segment].first_point = first_point;
	virtual_scan_segment_classes->segment_features[segment].last_point = last_point;
	virtual_scan_segment_classes->segment_features[segment].farthest_point = farthest_point;
	virtual_scan_segment_classes->segment_features[segment].width = width;
	virtual_scan_segment_classes->segment_features[segment].length = length;
	virtual_scan_segment_classes->segment_features[segment].segment_class = segment_class;
}


bool
segment_is_mass_point(virtual_scan_segment_t segment, carmen_point_t centroid)
{
	double max_distance_to_centroid = 0.0;
	for (int i = 0; i < segment.num_points; i++)
	{
		double distance = DIST2D(centroid, segment.points[i]);
		if (distance > max_distance_to_centroid)
			max_distance_to_centroid = distance;
	}

	if (max_distance_to_centroid < PEDESTRIAN_RADIUS)
		return (true);
	else
		return (false);
}


carmen_point_t
get_point_farthest_to_the_line_that_connects_the_first_to_the_last_point(virtual_scan_segment_t segment, carmen_point_t first_point, carmen_point_t last_point,
		double &maximum_distance_to_line_segment)
{
	carmen_point_t farthest_point = first_point;
	for (int j = 1; j < segment.num_points - 1; j++)
	{
		carmen_point_t point = segment.points[j];
		int point_type;
		carmen_point_t point_within_line_segment = distance_from_point_to_line_segment_vw(&point_type, first_point, last_point, point);
		if (point_type == POINT_WITHIN_SEGMENT)
		{
			double distance = DIST2D(point, point_within_line_segment);
			if (distance > maximum_distance_to_line_segment)
			{
				maximum_distance_to_line_segment = distance;
				farthest_point = point;
			}
		}
	}

	return (farthest_point);
}


bool
segment_is_i_shaped(carmen_point_t first_point, carmen_point_t last_point, carmen_point_t farthest_point, double maximum_distance_to_line_segment,
		double &width, double &length)
{
	double dist_farthest_first = DIST2D(farthest_point, first_point);
	double dist_farthest_last = DIST2D(farthest_point, last_point);
	if (dist_farthest_first > dist_farthest_last)
	{
		width = dist_farthest_last;
		length = dist_farthest_first;
	}
	else
	{
		width = dist_farthest_first;
		length = dist_farthest_last;
	}

	if ((maximum_distance_to_line_segment / length) < L_SMALL_SEGMENT_AS_A_PROPORTION_OF_THE_LARGE)
		return (true);
	else
		return (false);
}


void
classify_segments(virtual_scan_segment_classes_t *virtual_scan_segments)
{
	virtual_scan_segment_classes_t *virtual_scan_segment_classes = virtual_scan_segments;
	virtual_scan_segment_classes->segment_features = (virtual_scan_segment_features_t *) malloc(sizeof(virtual_scan_segment_features_t) *
			virtual_scan_segment_classes->num_segments);

	double width = PEDESTRIAN_RADIUS, length = PEDESTRIAN_RADIUS;
	for (int i = 0; i < virtual_scan_segments->num_segments; i++)
	{
		virtual_scan_segment_t segment = virtual_scan_segment_classes->segment[i];

		carmen_point_t first_point = segment.points[0];
		carmen_point_t last_point = segment.points[segment.num_points - 1];
		carmen_point_t centroid = compute_segment_centroid(segment);
		double maximum_distance_to_line_segment = 0.0;
		carmen_point_t farthest_point = get_point_farthest_to_the_line_that_connects_the_first_to_the_last_point(segment, first_point, last_point,
				maximum_distance_to_line_segment);

		int segment_class;
		if (segment_is_mass_point(segment, centroid))
			segment_class = MASS_POINT;
		else if (segment_is_i_shaped(first_point, last_point, farthest_point, maximum_distance_to_line_segment, width, length))
			segment_class = I_SHAPED;
		else
			segment_class = L_SHAPED;

		set_segment_features(virtual_scan_segment_classes, i, first_point, last_point,
				farthest_point, width, length, segment_class, centroid);
	}
}


int
is_last_box_model_hypotheses_empty(virtual_scan_box_model_hypotheses_t *box_model_hypotheses)
{
	int i = box_model_hypotheses->last_box_model_hypotheses;
	return (box_model_hypotheses->box_model_hypotheses[i].num_boxes == 0);
}


virtual_scan_box_model_t *
virtual_scan_append_box(virtual_scan_box_models_t *models, virtual_scan_segment_t box_points)
{
	int n = models->num_boxes + 1;
	models->num_boxes = n;

	if (n == 1)
	{
		models->box = (virtual_scan_box_model_t *) malloc(sizeof(virtual_scan_box_model_t));
		models->box_points = (virtual_scan_segment_t *) malloc(sizeof(virtual_scan_segment_t));
	}
	else
	{
		models->box = (virtual_scan_box_model_t *) realloc(models->box, sizeof(virtual_scan_box_model_t) * n);
		models->box_points = (virtual_scan_segment_t *) realloc(models->box_points, sizeof(virtual_scan_segment_t) * n);
	}

	models->box_points[n - 1] = box_points;

	virtual_scan_box_model_t *box = (models->box + n - 1);
	memset(box, '\0', sizeof(virtual_scan_box_model_t));

	return (box);
}


virtual_scan_box_model_hypotheses_t *
virtual_scan_new_box_model_hypotheses(int length)
{
	virtual_scan_box_model_hypotheses_t *hypotheses = (virtual_scan_box_model_hypotheses_t *) malloc(sizeof(virtual_scan_box_model_hypotheses_t));
	hypotheses->num_box_model_hypotheses = length;
	hypotheses->box_model_hypotheses = (virtual_scan_box_models_t *) calloc(length, sizeof(virtual_scan_box_models_t));
	hypotheses->last_box_model_hypotheses = 0;
	return hypotheses;
}


virtual_scan_box_models_t *
virtual_scan_get_empty_box_models(virtual_scan_box_model_hypotheses_t *hypotheses)
{
	return (hypotheses->box_model_hypotheses + hypotheses->last_box_model_hypotheses);
}


virtual_scan_box_models_t *
virtual_scan_get_box_models(virtual_scan_box_model_hypotheses_t *hypotheses, int i)
{
	return (hypotheses->box_model_hypotheses + i);
}


void
append_pedestrian_to_box_models(virtual_scan_box_models_t *box_models, virtual_scan_segment_t box_points, carmen_point_t centroid)
{
	virtual_scan_box_model_t *box = virtual_scan_append_box(box_models, box_points);
	box->c = PEDESTRIAN;
	box->x = centroid.x;
	box->y = centroid.y;
	box->width = 2.0 * PEDESTRIAN_RADIUS;
	box->length = 2.0 * PEDESTRIAN_RADIUS;
	box->theta = 0.0;
}


void
append_l_object(virtual_scan_box_models_t *box_models, virtual_scan_segment_t box_points, carmen_point_t farthest_point,
		int category, virtual_scan_category_t categories[], double theta, double theta2)
{
	carmen_position_t p1 = {farthest_point.x + categories[category].length * cos(theta),
							farthest_point.y + categories[category].length * sin(theta)};
	carmen_position_t p2 = {farthest_point.x + categories[category].width * cos(theta2),
							farthest_point.y + categories[category].width * sin(theta2)};
	carmen_position_t length_center_position;
	length_center_position.x = (p1.x + p2.x) / 2.0;
	length_center_position.y = (p1.y + p2.y) / 2.0;

	virtual_scan_box_model_t *box = virtual_scan_append_box(box_models, box_points);
	box->c = categories[category].category;
	box->width = categories[category].width;
	box->length = categories[category].length;
	box->x = length_center_position.x;
	box->y = length_center_position.y;
	box->theta = theta;
}


void
append_i_object(virtual_scan_box_models_t *box_models, virtual_scan_segment_t box_points, carmen_point_t first_point,
		int category, virtual_scan_category_t categories[], double theta, double theta2)
{
	carmen_position_t p1 = {first_point.x + categories[category].length * cos(theta),
							first_point.y + categories[category].length * sin(theta)};
	carmen_position_t p2 = {first_point.x + categories[category].width * cos(theta2),
							first_point.y + categories[category].width * sin(theta2)};
	carmen_position_t length_center_position;
	length_center_position.x = (p1.x + p2.x) / 2.0;
	length_center_position.y = (p1.y + p2.y) / 2.0;

	virtual_scan_box_model_t *box = virtual_scan_append_box(box_models, box_points);
	box->c = categories[category].category;
	box->width = categories[category].width;
	box->length = categories[category].length;
	box->x = length_center_position.x;
	box->y = length_center_position.y;
	box->theta = theta;
}


void
append_l_shaped_objects_to_box_models(virtual_scan_box_models_t *box_models, virtual_scan_segment_t box_points,
		virtual_scan_segment_features_t segment_features, virtual_scan_category_t categories[])
{
	carmen_point_t first_point = segment_features.first_point;
	carmen_point_t last_point = segment_features.last_point;
	carmen_point_t farthest_point = segment_features.farthest_point;
//	for (int category = 0; category < 3; category++)
	for (int category = 1; category < 2; category++)
	{
		double theta, theta2, l, w;

		l = DIST2D(farthest_point, first_point);
		w = DIST2D(farthest_point, last_point);
		if ((categories[category].length > l) &&
			(categories[category].width > (0.7 * w)) && (categories[category].width < (1.8 * w)))
		{
			theta = atan2(first_point.y - farthest_point.y, first_point.x - farthest_point.x);
			theta2 = atan2(last_point.y - farthest_point.y, last_point.x - farthest_point.x);
			append_l_object(box_models, box_points, farthest_point, category, categories, theta, theta2);
		}

		l = DIST2D(farthest_point, last_point);
		w = DIST2D(farthest_point, first_point);
		if ((categories[category].length > l) &&
			(categories[category].width > (0.7 * w)) && (categories[category].width < (1.8 * w)))
		{
			theta = atan2(last_point.y - farthest_point.y, last_point.x - farthest_point.x);
			theta2 = atan2(first_point.y - farthest_point.y, first_point.x - farthest_point.x);
			append_l_object(box_models, box_points, farthest_point, category, categories, theta, theta2);
		}
	}
}


void
append_i_shaped_objects_to_box_models(virtual_scan_box_models_t *box_models, virtual_scan_segment_t box_points,
		virtual_scan_segment_features_t segment_features, virtual_scan_category_t categories[])
{
	carmen_point_t first_point = segment_features.first_point;
	carmen_point_t last_point = segment_features.last_point;
	for (int category = 1; category < 2; category++)
	{
		double theta, theta2;

		double l = DIST2D(first_point, last_point);
		if ((l < categories[category].length) &&
			(l > categories[category].width * 1.5))
		{
			theta = atan2(last_point.y - first_point.y, last_point.x - first_point.x);
			theta2 = carmen_normalize_theta(theta - M_PI / 2.0);
			append_i_object(box_models, box_points, first_point, category, categories, theta, theta2);

			theta = atan2(first_point.y - last_point.y, first_point.x - last_point.x);
			theta2 = carmen_normalize_theta(theta + M_PI / 2.0);
			append_i_object(box_models, box_points, last_point, category, categories, theta, theta2);
		}
		else if (l <= categories[category].width * 1.5)
		{
			theta = carmen_normalize_theta(atan2(last_point.y - first_point.y, last_point.x - first_point.x) - M_PI / 2.0);
			theta2 = carmen_normalize_theta(theta + M_PI / 2.0);
			append_i_object(box_models, box_points, first_point, category, categories, theta, theta2);

			theta2 = carmen_normalize_theta(theta - M_PI / 2.0);
			append_i_object(box_models, box_points, last_point, category, categories, theta, theta2);
		}
	}
}


virtual_scan_box_model_hypotheses_t *
virtual_scan_fit_box_models(virtual_scan_segment_classes_t *virtual_scan_segment_classes)
{
	virtual_scan_category_t categories[] = {{BUS, 2.5, 15.0}, {CAR, 1.5, 4.5}, {BIKE, 0.5, 2.1}}; // Trung-Dung Vu Thesis

	int num_segments = virtual_scan_segment_classes->num_segments;
	virtual_scan_box_model_hypotheses_t *box_model_hypotheses = virtual_scan_new_box_model_hypotheses(num_segments);
	box_model_hypotheses->timestamp = virtual_scan_segment_classes->timestamp;
	for (int i = 0; i < num_segments; i++)
	{
		int segment_class = virtual_scan_segment_classes->segment_features[i].segment_class;
		virtual_scan_segment_t box_points = virtual_scan_segment_classes->segment[i];

		virtual_scan_box_models_t *box_models = virtual_scan_get_empty_box_models(box_model_hypotheses);
//		if (segment_class == MASS_POINT)
//			append_pedestrian_to_box_models(box_models, box_points, virtual_scan_segment_classes->segment_features[i].centroid);
		if (segment_class == L_SHAPED) // L-shape segments segments will generate bus and car hypotheses
			append_l_shaped_objects_to_box_models(box_models, box_points, virtual_scan_segment_classes->segment_features[i], categories);
		if (segment_class == I_SHAPED) // I-shape segments segments will generate bus, car and bike hypotheses
			append_i_shaped_objects_to_box_models(box_models, box_points, virtual_scan_segment_classes->segment_features[i], categories);

		if (!is_last_box_model_hypotheses_empty(box_model_hypotheses))
			box_model_hypotheses->last_box_model_hypotheses++;
	}

	return (box_model_hypotheses);
}


int
virtual_scan_num_box_models(virtual_scan_box_model_hypotheses_t *virtual_scan_box_model_hypotheses)
{
	int total = 0;

	virtual_scan_box_models_t *hypotheses = virtual_scan_box_model_hypotheses->box_model_hypotheses;
	for (int i = 0, m = virtual_scan_box_model_hypotheses->num_box_model_hypotheses; i < m; i++)
		total += hypotheses[i].num_boxes;

	return total;
}


void
virtual_scan_free_box_model_hypotheses(virtual_scan_box_model_hypotheses_t *virtual_scan_box_model_hypotheses)
{
	for (int i = 0; i < virtual_scan_box_model_hypotheses->num_box_model_hypotheses; i++)
	{
		free(virtual_scan_box_model_hypotheses->box_model_hypotheses[i].box);
		free(virtual_scan_box_model_hypotheses->box_model_hypotheses[i].box_points);
	}
	free(virtual_scan_box_model_hypotheses->box_model_hypotheses);
	free(virtual_scan_box_model_hypotheses);
}


void
virtual_scan_free_segment_classes(virtual_scan_segment_classes_t *virtual_scan_segment_classes)
{
	if (virtual_scan_segment_classes != NULL)
	{
		for (int i = 0; i < virtual_scan_segment_classes->num_segments; i++)
			free(virtual_scan_segment_classes->segment[i].points);

		free(virtual_scan_segment_classes->segment);
		free(virtual_scan_segment_classes->segment_features);
		free(virtual_scan_segment_classes);
	}
}


void
virtual_scan_free_scan_extended(virtual_scan_extended_t *virtual_scan_extended)
{
	if (virtual_scan_extended != NULL)
	{
		free(virtual_scan_extended->points);
		free(virtual_scan_extended);
	}
}


virtual_scan_segment_classes_t *
virtual_scan_extract_segments(virtual_scan_extended_t *virtual_scan_extended)
{
	virtual_scan_extended_t *virtual_scan_extended_filtered = filter_virtual_scan(virtual_scan_extended);
	virtual_scan_segment_classes_t *virtual_scan_segments = segment_virtual_scan(virtual_scan_extended_filtered);
	classify_segments(virtual_scan_segments);

	virtual_scan_free_scan_extended(virtual_scan_extended_filtered);

	return (virtual_scan_segments);
}


void 
create_hypothesis_vertex(int h, int i, int j, virtual_scan_neighborhood_graph_t* neighborhood_graph,
		virtual_scan_box_model_hypotheses_t* virtual_scan_box_model_hypotheses)
{
	neighborhood_graph->box_model_hypothesis[h] = (virtual_scan_box_model_hypothesis_t *) calloc(1, sizeof(virtual_scan_box_model_hypothesis_t));
	neighborhood_graph->box_model_hypothesis[h]->hypothesis = virtual_scan_box_model_hypotheses->box_model_hypotheses[i].box[j];
	neighborhood_graph->box_model_hypothesis[h]->hypothesis_points = virtual_scan_box_model_hypotheses->box_model_hypotheses[i].box_points[j];
	neighborhood_graph->box_model_hypothesis[h]->zi = g_zi;
	neighborhood_graph->box_model_hypothesis[h]->index = h;
	neighborhood_graph->box_model_hypothesis[h]->timestamp = virtual_scan_box_model_hypotheses->timestamp;
}


void
create_hypothesis_sibling_edges(int h, int previous_h, int num_boxes, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	neighborhood_graph->box_model_hypothesis_edges[h] =
			(virtual_scan_box_model_hypothesis_edges_t *) malloc(sizeof(virtual_scan_box_model_hypothesis_edges_t));
	neighborhood_graph->box_model_hypothesis_edges[h]->size = num_boxes - 1;
	if (neighborhood_graph->box_model_hypothesis_edges[h]->size != 0)
	{
		neighborhood_graph->box_model_hypothesis_edges[h]->edge = (int *) malloc((num_boxes - 1) * sizeof(int));
		neighborhood_graph->box_model_hypothesis_edges[h]->edge_type = (int *) malloc((num_boxes - 1) * sizeof(int));
		int e = 0;
		int reference_h = h - previous_h;
		for (int k = 0; k < num_boxes; k++)
		{
			if (k != reference_h)
			{
				neighborhood_graph->box_model_hypothesis_edges[h]->edge[e] = k + previous_h;
				neighborhood_graph->box_model_hypothesis_edges[h]->edge_type[e] = SIBLING_EDGE;
				e++;
			}
		}
	}
	else
	{
		neighborhood_graph->box_model_hypothesis_edges[h]->edge = NULL;
		neighborhood_graph->box_model_hypothesis_edges[h]->edge_type = NULL;
	}
}


bool
is_parent(int candidate_parent, int child, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	if (neighborhood_graph->box_model_hypothesis[candidate_parent]->hypothesis.c == neighborhood_graph->box_model_hypothesis[child]->hypothesis.c)
	{
		double distance = DIST2D(neighborhood_graph->box_model_hypothesis[candidate_parent]->hypothesis, neighborhood_graph->box_model_hypothesis[child]->hypothesis);
		double delta_t = neighborhood_graph->box_model_hypothesis[child]->timestamp - neighborhood_graph->box_model_hypothesis[candidate_parent]->timestamp;
		int delta_zi = neighborhood_graph->box_model_hypothesis[child]->zi - neighborhood_graph->box_model_hypothesis[candidate_parent]->zi;
		if (delta_zi < 0)
			delta_zi += NUMBER_OF_FRAMES_T;

		if ((distance < delta_t * VMAX) && (delta_zi <= 2))
			return (true);
	}

	return (false);
}


void
create_hypothesis_parent_child_edges(int child, virtual_scan_neighborhood_graph_t *neighborhood_graph, double current_timestamp)
{
	int num_candidate_parent = 0;
	while ((num_candidate_parent < neighborhood_graph->size) && (neighborhood_graph->box_model_hypothesis[num_candidate_parent]->timestamp != current_timestamp))
		num_candidate_parent++;

	for (int candidate_parent = 0; candidate_parent < num_candidate_parent; candidate_parent++)
	{
		if (is_parent(candidate_parent, child, neighborhood_graph))
		{
			int parent = candidate_parent;

			int e = neighborhood_graph->box_model_hypothesis_edges[child]->size;
			neighborhood_graph->box_model_hypothesis_edges[child]->edge = (int *) realloc(neighborhood_graph->box_model_hypothesis_edges[child]->edge,
					(e + 1) * sizeof(int));
			neighborhood_graph->box_model_hypothesis_edges[child]->edge_type = (int *) realloc(neighborhood_graph->box_model_hypothesis_edges[child]->edge_type,
					(e + 1) * sizeof(int));

			neighborhood_graph->box_model_hypothesis_edges[child]->edge[e] = parent;
			neighborhood_graph->box_model_hypothesis_edges[child]->edge_type[e] = PARENT_EDGE;

			neighborhood_graph->box_model_hypothesis_edges[child]->size += 1;

			e = neighborhood_graph->box_model_hypothesis_edges[parent]->size;
			neighborhood_graph->box_model_hypothesis_edges[parent]->edge = (int *) realloc(neighborhood_graph->box_model_hypothesis_edges[parent]->edge,
					(e + 1) * sizeof(int));
			neighborhood_graph->box_model_hypothesis_edges[parent]->edge_type = (int *) realloc(neighborhood_graph->box_model_hypothesis_edges[parent]->edge_type,
					(e + 1) * sizeof(int));

			neighborhood_graph->box_model_hypothesis_edges[parent]->edge[e] = child;
			neighborhood_graph->box_model_hypothesis_edges[parent]->edge_type[e] = CHILD_EDGE;

			neighborhood_graph->box_model_hypothesis_edges[parent]->size += 1;
		}
	}
}


void
free_neighborhood_graph_vextexes(virtual_scan_neighborhood_graph_t *neighborhood_graph, int vextexes_to_remove)
{
	for (int i = 0; i < vextexes_to_remove; i++)
	{
		free(neighborhood_graph->box_model_hypothesis[i]);
		free(neighborhood_graph->box_model_hypothesis_edges[i]->edge);
		free(neighborhood_graph->box_model_hypothesis_edges[i]->edge_type);
		free(neighborhood_graph->box_model_hypothesis_edges[i]);
	}
}


void
move_neighborhood_graph_vextexes(virtual_scan_neighborhood_graph_t *neighborhood_graph, int vextexes_to_remove)
{
	for (int i = 0; i < (neighborhood_graph->size - vextexes_to_remove); i++)
	{
		neighborhood_graph->box_model_hypothesis[i] = neighborhood_graph->box_model_hypothesis[i + vextexes_to_remove];
		neighborhood_graph->box_model_hypothesis_edges[i] = neighborhood_graph->box_model_hypothesis_edges[i + vextexes_to_remove];
	}
}


void
remove_edge(virtual_scan_neighborhood_graph_t *neighborhood_graph, int vertex, int edge)
{
	for (int i = edge; i < neighborhood_graph->box_model_hypothesis_edges[vertex]->size - 1; i++)
	{
		neighborhood_graph->box_model_hypothesis_edges[vertex]->edge[i] = neighborhood_graph->box_model_hypothesis_edges[vertex]->edge[i + 1];
		neighborhood_graph->box_model_hypothesis_edges[vertex]->edge_type[i] = neighborhood_graph->box_model_hypothesis_edges[vertex]->edge_type[i + 1];
	}

	neighborhood_graph->box_model_hypothesis_edges[vertex]->size -= 1;
	neighborhood_graph->box_model_hypothesis_edges[vertex]->edge = (int *) realloc(neighborhood_graph->box_model_hypothesis_edges[vertex]->edge,
			neighborhood_graph->box_model_hypothesis_edges[vertex]->size * sizeof(int));
	neighborhood_graph->box_model_hypothesis_edges[vertex]->edge_type = (int *) realloc(neighborhood_graph->box_model_hypothesis_edges[vertex]->edge_type,
			neighborhood_graph->box_model_hypothesis_edges[vertex]->size * sizeof(int));
}


void
rename_neighborhood_graph_vextexes_ids(virtual_scan_neighborhood_graph_t *neighborhood_graph, int vextexes_to_remove)
{
	for (int i = 0; i < (neighborhood_graph->size - vextexes_to_remove); i++)
	{
		neighborhood_graph->box_model_hypothesis[i]->index -= vextexes_to_remove;
		if (neighborhood_graph->box_model_hypothesis[i]->index < 0)
			carmen_die("Error: neighborhood_graph->box_model_hypothesis[i]->index < 0 in rename_neighborhood_graph_vextexes_ids().\n");

		int j = 0;
		while (j < neighborhood_graph->box_model_hypothesis_edges[i]->size)
		{
			neighborhood_graph->box_model_hypothesis_edges[i]->edge[j] -= vextexes_to_remove;
			if (neighborhood_graph->box_model_hypothesis_edges[i]->edge[j] < 0)
				remove_edge(neighborhood_graph, i, j);
			else
				j++;
		}
	}
}


int
remove_graph_vertexes_of_victim_timestamp(double victim_timestamp, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	int vextexes_to_remove = 0;
	while ((vextexes_to_remove < neighborhood_graph->size) && (neighborhood_graph->box_model_hypothesis[vextexes_to_remove]->timestamp == victim_timestamp))
		vextexes_to_remove++;

	free_neighborhood_graph_vextexes(neighborhood_graph, vextexes_to_remove);
	move_neighborhood_graph_vextexes(neighborhood_graph, vextexes_to_remove);
	rename_neighborhood_graph_vextexes_ids(neighborhood_graph, vextexes_to_remove);

	neighborhood_graph->size -= vextexes_to_remove;
	neighborhood_graph->box_model_hypothesis =
			(virtual_scan_box_model_hypothesis_t **) realloc(neighborhood_graph->box_model_hypothesis, neighborhood_graph->size * sizeof(virtual_scan_box_model_hypothesis_t *));
	neighborhood_graph->box_model_hypothesis_edges =
			(virtual_scan_box_model_hypothesis_edges_t **) realloc(neighborhood_graph->box_model_hypothesis_edges, neighborhood_graph->size * sizeof(virtual_scan_box_model_hypothesis_edges_t *));

	return (vextexes_to_remove);
}


virtual_scan_box_model_hypothesis_t *
update_track_according_to_new_graph(virtual_scan_track_t *track, int vextexes_removed_from_graph)
{
	int i = 0;
	while (i < track->size)
	{
		track->box_model_hypothesis[i].index -= vextexes_removed_from_graph;
		if (track->box_model_hypothesis[i].index < 0) // Remove hypothesis
		{
			for (int j = i; j < track->size - 1; j++)
				track->box_model_hypothesis[j] = track->box_model_hypothesis[j + 1];

			track->size--;
			track->box_model_hypothesis = (virtual_scan_box_model_hypothesis_t *) realloc(track->box_model_hypothesis,
					track->size * sizeof(virtual_scan_box_model_hypothesis_t));
		}
		else
			i++;
	}

	if (track->size != 0)
		return (track->box_model_hypothesis);
	else
		return (NULL);
}


virtual_scan_track_set_t *
update_best_track_set_due_to_vertexes_removed_from_graph(virtual_scan_track_set_t *best_track_set, int vextexes_removed_from_graph, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	if (best_track_set == NULL)
		return (NULL);

	int i = 0;
	while (i < best_track_set->size)
	{
		virtual_scan_track_t *track = best_track_set->tracks[i];
		if (update_track_according_to_new_graph(track, vextexes_removed_from_graph) == NULL) // Remove track
		{
			for (int j = i; j < best_track_set->size - 1; j++)
				best_track_set->tracks[j] = best_track_set->tracks[j + 1];

			best_track_set->size--;
			best_track_set->tracks = (virtual_scan_track_t **) realloc(best_track_set->tracks, best_track_set->size * sizeof(virtual_scan_track_t *));
		}
		else
			i++;
	}

	if (best_track_set->size != 0)
	{
		memmove((void *) &(best_track_set->vertex_selected[0]), (void *) &(best_track_set->vertex_selected[vextexes_removed_from_graph]),
				neighborhood_graph->size * sizeof(bool));
		best_track_set->vertex_selected = (bool *) realloc(best_track_set->vertex_selected, neighborhood_graph->size * sizeof(bool));

		return (best_track_set);
	}
	else
	{
		free(best_track_set->vertex_selected);
		free(best_track_set);

		return (NULL);
	}
}


void
update_best_track_set_due_to_vertexes_added_to_graph(virtual_scan_track_set_t *best_track_set, int vextexes_added_to_graph, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	if (best_track_set == NULL)
		return;

	best_track_set->vertex_selected = (bool *) realloc(best_track_set->vertex_selected, (neighborhood_graph->size + vextexes_added_to_graph) * sizeof(bool));
	for (int i = neighborhood_graph->size; i < (neighborhood_graph->size + vextexes_added_to_graph); i++)
		best_track_set->vertex_selected[i] = false;
}


virtual_scan_neighborhood_graph_t *
first_neighborhood_graph_update(virtual_scan_box_model_hypotheses_t *virtual_scan_box_model_hypotheses)
{
	virtual_scan_neighborhood_graph_t *neighborhood_graph = (virtual_scan_neighborhood_graph_t *) malloc(sizeof(virtual_scan_neighborhood_graph_t));
	carmen_test_alloc(neighborhood_graph);
	neighborhood_graph->graph_id = 0;
	neighborhood_graph->last_frames_timetamps = (double *) malloc(NUMBER_OF_FRAMES_T * sizeof(double));

	int num_hypotheses = 0;
	for (int i = 0; i < virtual_scan_box_model_hypotheses->num_box_model_hypotheses; i++)
		for (int j = 0; j < virtual_scan_box_model_hypotheses->box_model_hypotheses[i].num_boxes; j++)
			num_hypotheses++;

	neighborhood_graph->size = 0;
	neighborhood_graph->box_model_hypothesis = (virtual_scan_box_model_hypothesis_t **) malloc(num_hypotheses * sizeof(virtual_scan_box_model_hypothesis_t *));
	neighborhood_graph->box_model_hypothesis_edges =
			(virtual_scan_box_model_hypothesis_edges_t **) malloc(num_hypotheses * sizeof(virtual_scan_box_model_hypothesis_edges_t *));

	int h = 0;
	for (int i = 0; i < virtual_scan_box_model_hypotheses->num_box_model_hypotheses; i++)
	{
		int num_boxes = virtual_scan_box_model_hypotheses->box_model_hypotheses[i].num_boxes;
		int previous_h = h;
		for (int j = 0; j < num_boxes; j++, h++)
			create_hypothesis_vertex(h, i, j, neighborhood_graph, virtual_scan_box_model_hypotheses);
		h = previous_h;
		for (int j = 0; j < num_boxes; j++, h++)
			create_hypothesis_sibling_edges(h, previous_h, num_boxes, neighborhood_graph);
	}
	neighborhood_graph->size = num_hypotheses;

	neighborhood_graph->number_of_frames_filled = 0;
	neighborhood_graph->last_frames_timetamps[neighborhood_graph->number_of_frames_filled] = virtual_scan_box_model_hypotheses->timestamp;
	neighborhood_graph->number_of_frames_filled = 1;

	return (neighborhood_graph);
}


virtual_scan_neighborhood_graph_t *
neighborhood_graph_update(virtual_scan_box_model_hypotheses_t *virtual_scan_box_model_hypotheses, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	neighborhood_graph->graph_id += 1;

	int num_hypotheses = 0;
	for (int i = 0; i < virtual_scan_box_model_hypotheses->num_box_model_hypotheses; i++)
		for (int j = 0; j < virtual_scan_box_model_hypotheses->box_model_hypotheses[i].num_boxes; j++)
			num_hypotheses++;

	int first_new_vertex_to_add = neighborhood_graph->size;
	neighborhood_graph->box_model_hypothesis =
			(virtual_scan_box_model_hypothesis_t **) realloc(neighborhood_graph->box_model_hypothesis, (neighborhood_graph->size + num_hypotheses) * sizeof(virtual_scan_box_model_hypothesis_t *));
	neighborhood_graph->box_model_hypothesis_edges =
			(virtual_scan_box_model_hypothesis_edges_t **) realloc(neighborhood_graph->box_model_hypothesis_edges, (neighborhood_graph->size + num_hypotheses) * sizeof(virtual_scan_box_model_hypothesis_edges_t *));

	update_best_track_set_due_to_vertexes_added_to_graph(best_track_set, num_hypotheses, neighborhood_graph);

	int h = first_new_vertex_to_add; // Each vertex hold an hypothesis
	for (int i = 0; i < virtual_scan_box_model_hypotheses->num_box_model_hypotheses; i++)
	{
		int num_boxes = virtual_scan_box_model_hypotheses->box_model_hypotheses[i].num_boxes;
		int previous_h = h;
		for (int j = 0; j < num_boxes; j++, h++)
			create_hypothesis_vertex(h, i, j, neighborhood_graph, virtual_scan_box_model_hypotheses);
		h = previous_h;
		for (int j = 0; j < num_boxes; j++, h++)
		{
			create_hypothesis_sibling_edges(h, previous_h, num_boxes, neighborhood_graph);
			create_hypothesis_parent_child_edges(h, neighborhood_graph, virtual_scan_box_model_hypotheses->timestamp);
		}
	}
	neighborhood_graph->size += num_hypotheses;

	if (neighborhood_graph->number_of_frames_filled == NUMBER_OF_FRAMES_T)
	{
		double victim_timestamp = neighborhood_graph->last_frames_timetamps[0];
		for (int i = 0; i < NUMBER_OF_FRAMES_T - 1; i++)
			neighborhood_graph->last_frames_timetamps[i] = neighborhood_graph->last_frames_timetamps[i + 1];

		int vextexes_removed_from_graph = remove_graph_vertexes_of_victim_timestamp(victim_timestamp, neighborhood_graph);
		best_track_set = update_best_track_set_due_to_vertexes_removed_from_graph(best_track_set, vextexes_removed_from_graph, neighborhood_graph);

		neighborhood_graph->number_of_frames_filled -= 1;
	}

	neighborhood_graph->last_frames_timetamps[neighborhood_graph->number_of_frames_filled] = virtual_scan_box_model_hypotheses->timestamp;
	neighborhood_graph->number_of_frames_filled += 1;

	return (neighborhood_graph);
}


void
print_neighborhood_graph(virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	FILE *graph = fopen("graph.txt", "a");
	for (int i = 0; i < neighborhood_graph->size; i++)
	{
		fprintf(graph, "Graph Id %d, h %d %c -", neighborhood_graph->graph_id, i, neighborhood_graph->box_model_hypothesis[i]->hypothesis.c);
		for (int j = 0; j < neighborhood_graph->box_model_hypothesis_edges[i]->size; j++)
			fprintf(graph, " %c(%d, %d)", neighborhood_graph->box_model_hypothesis_edges[i]->edge_type[j], i, neighborhood_graph->box_model_hypothesis_edges[i]->edge[j]);
		fprintf(graph, "\n");
	}
	fprintf(graph, "\n");
	fclose(graph);
}


virtual_scan_neighborhood_graph_t *
virtual_scan_update_neighborhood_graph(virtual_scan_neighborhood_graph_t *neighborhood_graph, virtual_scan_box_model_hypotheses_t *virtual_scan_box_model_hypotheses)
{
	if (virtual_scan_box_model_hypotheses == NULL)
		return (neighborhood_graph);

	if (neighborhood_graph == NULL)
		neighborhood_graph = first_neighborhood_graph_update(virtual_scan_box_model_hypotheses);
	else
		neighborhood_graph = neighborhood_graph_update(virtual_scan_box_model_hypotheses, neighborhood_graph);

//	print_neighborhood_graph(neighborhood_graph);

	return (neighborhood_graph);
}


bool
do_not_have_sibling_in_track_set(virtual_scan_box_model_hypothesis_edges_t *hypothesis_edges, virtual_scan_track_set_t *track_set)
{
	for (int k = 0; k < hypothesis_edges->size; k++)
	{
		if (hypothesis_edges->edge_type[k] == SIBLING_EDGE)
		{
			for (int i = 0; i < track_set->size; i++)
			{
				for (int j = 0; j < track_set->tracks[i]->size; j++)
				{
					int h = track_set->tracks[i]->box_model_hypothesis[j].index;
					if (hypothesis_edges->edge[k] == h)
						return (false);
				}
			}
		}
	}

	return (true);
}


int *
get_v_star(int &size, virtual_scan_track_set_t *track_set, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	size = 0;

	if (track_set != NULL)
	{
		for (int i = 0; i < neighborhood_graph->size; i++)
		{
			if (!track_set->vertex_selected[i])
				if (do_not_have_sibling_in_track_set(neighborhood_graph->box_model_hypothesis_edges[i], track_set))
					size++;
		}

		if (size != 0)
		{
			int *v_star = (int *) malloc(size * sizeof(int));
			for (int i = 0, j = 0; i < neighborhood_graph->size; i++)
			{
				if (!track_set->vertex_selected[i])
				{
					if (do_not_have_sibling_in_track_set(neighborhood_graph->box_model_hypothesis_edges[i], track_set))
					{
						v_star[j] = i;
						j++;
					}
				}
			}
			return (v_star);
		}
		else
			return (NULL);
	}
	else
	{
		size = neighborhood_graph->size;
		if (size != 0)
		{
			int *v_star = (int *) malloc(size * sizeof(int));
			for (int i = 0; i < size; i++)
				v_star[i] = i;

			return (v_star);
		}
		else
			return (NULL);
	}
}


void
free_track(virtual_scan_track_t *track)
{
	free(track->box_model_hypothesis);
	free(track);
}


void
free_track_set(virtual_scan_track_set_t *track_set_victim)
{
	if (track_set_victim != NULL)
	{
		for (int i = 0; i < track_set_victim->size; i++)
			free_track(track_set_victim->tracks[i]);

		free(track_set_victim->tracks);
		free(track_set_victim->vertex_selected);
		free(track_set_victim);
	}
}


carmen_point_t
vect2d(carmen_position_t p1, carmen_position_t p2)
{
    carmen_point_t temp;

    temp.x = (p2.x - p1.x);
    temp.y = -(p2.y - p1.y);

    return (temp);
}


static carmen_position_t
rotate_rectangle_vertice(carmen_position_t unrotated_vertice, carmen_position_t rectangle_center, double rectangle_theta)
{
	carmen_position_t rotated_vertice;

	double cos_theta = cos(rectangle_theta);
	double sin_theta = sin(rectangle_theta);

	rotated_vertice.x = rectangle_center.x + cos_theta * (unrotated_vertice.x - rectangle_center.x) - sin_theta * (unrotated_vertice.y - rectangle_center.y);
	rotated_vertice.y = rectangle_center.y + sin_theta * (unrotated_vertice.x - rectangle_center.x) + cos_theta * (unrotated_vertice.y - rectangle_center.y);

	return (rotated_vertice);
}


int
point_inside_scaled_rectangle(carmen_point_t point, virtual_scan_box_model_t hypothesis, double scale)
{	// https://stackoverflow.com/questions/2752725/finding-whether-a-point-lies-inside-a-rectangle-or-not
	carmen_position_t A, B, C, D, m;

	double length = hypothesis.length * scale;
	double width = hypothesis.width * scale;
	carmen_position_t hypothesis_center = {hypothesis.x, hypothesis.y};

	m = {point.x, point.y};

	A.x = hypothesis.x - length / 2.0;
	A.y = hypothesis.y - width / 2.0;
	A = rotate_rectangle_vertice(A, hypothesis_center, hypothesis.theta);

	B.x = hypothesis.x + length / 2.0;
	B.y = hypothesis.y - width / 2.0;
	B = rotate_rectangle_vertice(B, hypothesis_center, hypothesis.theta);

	C.x = hypothesis.x + length / 2.0;
	C.y = hypothesis.y + width / 2.0;
	C = rotate_rectangle_vertice(C, hypothesis_center, hypothesis.theta);

	D.x = hypothesis.x - length / 2.0;
	D.y = hypothesis.y + width / 2.0;
	D = rotate_rectangle_vertice(D, hypothesis_center, hypothesis.theta);

	carmen_point_t AB = vect2d(A, B);
	double C1 = -(AB.y * A.x + AB.x * A.y);
	double D1 = (AB.y * m.x + AB.x * m.y) + C1;

	carmen_point_t BC = vect2d(B, C);
	double C2 = -(BC.y * B.x + BC.x * B.y);
	double D2 = (BC.y * m.x + BC.x * m.y) + C2;

	carmen_point_t CD = vect2d(C, D);
	double C3 = -(CD.y * C.x + CD.x * C.y);
	double D3 = (CD.y * m.x + CD.x * m.y) + C3;

	carmen_point_t DA = vect2d(D, A);
	double C4 = -(DA.y * A.x + DA.x * A.y);
	double D4 = (DA.y * m.x + DA.x * m.y) + C4;

	if ((D1 > 0.0) && (D4 > 0.0) && (D2 > 0.0) && (D3 > 0.0))
		return (1);
	else
		return (0);
}


void
get_points_inside_and_outside_scaled_rectangle(carmen_point_t *&points_inside_rectangle, carmen_point_t *&points_outside_rectangle,
		int &num_points_inside_rectangle, int &num_points_outside_rectangle,
		virtual_scan_box_model_hypothesis_t *box_model_hypothesis, double scale)
{
	int num_points = g_virtual_scan_extended[box_model_hypothesis->zi]->num_points;
	carmen_point_t *point = g_virtual_scan_extended[box_model_hypothesis->zi]->points;

	points_inside_rectangle = (carmen_point_t *) malloc(sizeof(carmen_point_t) * num_points);
	points_outside_rectangle = (carmen_point_t *) malloc(sizeof(carmen_point_t) * num_points);
	num_points_inside_rectangle = num_points_outside_rectangle = 0;
	for (int i = 0; i < num_points; i++)
	{
		if (point_inside_scaled_rectangle(point[i], box_model_hypothesis->hypothesis, scale))
		{
			points_inside_rectangle[num_points_inside_rectangle] = point[i];
			num_points_inside_rectangle++;
		}
		else
		{
			points_outside_rectangle[num_points_outside_rectangle] = point[i];
			num_points_outside_rectangle++;
		}
	}
}


void
get_points_inside_and_outside_scaled_rectangle(carmen_point_t *&points_inside_rectangle, carmen_point_t *&points_outside_rectangle,
		int &num_points_inside_rectangle, int &num_points_outside_rectangle, carmen_point_t *point, int num_points,
		virtual_scan_box_model_hypothesis_t *box_model_hypothesis, double scale)
{
	points_inside_rectangle = (carmen_point_t *) malloc(sizeof(carmen_point_t) * num_points);
	points_outside_rectangle = (carmen_point_t *) malloc(sizeof(carmen_point_t) * num_points);
	num_points_inside_rectangle = num_points_outside_rectangle = 0;
	for (int i = 0; i < num_points; i++)
	{
		if (point_inside_scaled_rectangle(point[i], box_model_hypothesis->hypothesis, scale))
		{
			points_inside_rectangle[num_points_inside_rectangle] = point[i];
			num_points_inside_rectangle++;
		}
		else
		{
			points_outside_rectangle[num_points_outside_rectangle] = point[i];
			num_points_outside_rectangle++;
		}
	}
}


double
get_distance_to_hypothesis_rectangle(carmen_point_t zd_point, virtual_scan_box_model_hypothesis_t *box_model_hypothesis)
{
	virtual_scan_box_model_t hypothesis = box_model_hypothesis->hypothesis;
	carmen_position_t hypothesis_center = {hypothesis.x, hypothesis.y};
	carmen_position_t v, w;
	double d, new_d;

	v.x = hypothesis.x - hypothesis.length / 2.0;
	v.y = hypothesis.y - hypothesis.width / 2.0;
	v = rotate_rectangle_vertice(v, hypothesis_center, hypothesis.theta);

	w.x = hypothesis.x + hypothesis.length / 2.0;
	w.y = hypothesis.y - hypothesis.width / 2.0;
	w = rotate_rectangle_vertice(w, hypothesis_center, hypothesis.theta);
	d = distance_from_point_to_line_segment_vw(v, w, zd_point);

	w.x = hypothesis.x - hypothesis.length / 2.0;
	w.y = hypothesis.y + hypothesis.width / 2.0;
	w = rotate_rectangle_vertice(w, hypothesis_center, hypothesis.theta);
	new_d = distance_from_point_to_line_segment_vw(v, w, zd_point);
	if (new_d < d)
		d = new_d;

	v.x = hypothesis.x + hypothesis.length / 2.0;
	v.y = hypothesis.y + hypothesis.width / 2.0;
	v = rotate_rectangle_vertice(v, hypothesis_center, hypothesis.theta);
	new_d = distance_from_point_to_line_segment_vw(v, w, zd_point);
	if (new_d < d)
		d = new_d;

	w.x = hypothesis.x + hypothesis.length / 2.0;
	w.y = hypothesis.y - hypothesis.width / 2.0;
	w = rotate_rectangle_vertice(w, hypothesis_center, hypothesis.theta);
	new_d = distance_from_point_to_line_segment_vw(v, w, zd_point);
	if (new_d < d)
		d = new_d;

	return (d);
}


void
get_Zs_and_Zd_of_hypothesis(carmen_point_t *&Zs_in, int &Zs_in_size, carmen_point_t *&Zs_out, int &Zs_out_size,
		carmen_point_t *&Zd, int &Zd_size, virtual_scan_box_model_hypothesis_t *box_model_hypothesis)
{
	int num_points_inside_large_rectangle;
	carmen_point_t *points_inside_large_rectangle;

	get_points_inside_and_outside_scaled_rectangle(points_inside_large_rectangle, Zs_out,
			num_points_inside_large_rectangle, Zs_out_size, box_model_hypothesis, 1.3);

	get_points_inside_and_outside_scaled_rectangle(Zs_in, Zd, Zs_in_size, Zd_size,
			points_inside_large_rectangle, num_points_inside_large_rectangle, box_model_hypothesis, 0.7);

	free(points_inside_large_rectangle);
}


double
PM1(carmen_point_t *Zd, int Zd_size, virtual_scan_box_model_hypothesis_t *box_model_hypothesis)
{
	double sum = 0.0;
	for (int i = 0; i < Zd_size; i++)
		sum += get_distance_to_hypothesis_rectangle(Zd[i], box_model_hypothesis);

	return (sum);
}


double
PM2(carmen_point_t *Zs_out, int Zs_out_size, virtual_scan_box_model_hypothesis_t *box_model_hypothesis)
{
	carmen_position_t velodyne_pos = {g_virtual_scan_extended[box_model_hypothesis->zi]->velodyne_pos.x, g_virtual_scan_extended[box_model_hypothesis->zi]->velodyne_pos.y};
	carmen_rectangle_t rectangle = {box_model_hypothesis->hypothesis.x, box_model_hypothesis->hypothesis.y, box_model_hypothesis->hypothesis.theta,
			box_model_hypothesis->hypothesis.length, box_model_hypothesis->hypothesis.width};
	carmen_position_t nearest_intersection;

	double sum = 0.0;
	for (int i = 0; i < Zs_out_size; i++)
	{
		carmen_position_t point = {Zs_out[i].x, Zs_out[i].y};
		if (DIST2D(velodyne_pos, point) > DIST2D(velodyne_pos, rectangle))
			sum += (double) carmen_line_to_point_crossed_rectangle(&nearest_intersection, velodyne_pos, point, rectangle);
	}

	return (sum);
}


void
compute_hypothesis_posterior_probability_components(virtual_scan_box_model_hypothesis_t *box_model_hypothesis)
{
	if (box_model_hypothesis == NULL)
		return;

	carmen_point_t *Zs_out, *Zs_in, *Zd; int Zs_out_size, Zs_in_size, Zd_size;
	get_Zs_and_Zd_of_hypothesis(Zs_in, Zs_in_size, Zs_out, Zs_out_size, Zd, Zd_size, box_model_hypothesis);

	box_model_hypothesis->dn = PM1(Zd, Zd_size, box_model_hypothesis) / ((double) Zd_size + 1.0);
	box_model_hypothesis->c2 = PM2(Zs_out, Zs_out_size, box_model_hypothesis) / ((double) Zd_size + 1.0);
	box_model_hypothesis->c3 = 0.0;

	free(Zs_out); free(Zs_in); free(Zd);
}


void
compute_track_posterior_probability_components(virtual_scan_track_t *track)
{
	if (track == NULL)
		return;

	for (int h = 0; h < track->size; h++)
	{
		virtual_scan_box_model_hypothesis_t *box_model_hypothesis = &(track->box_model_hypothesis[h]);
		compute_hypothesis_posterior_probability_components(box_model_hypothesis);
	}
}


void
compute_track_set_posterior_probability_components(virtual_scan_track_set_t *track_set)
{
	if (track_set == NULL)
		return;

	for (int i = 0; i < track_set->size; i++)
	{
		virtual_scan_track_t *track = track_set->tracks[i];
		compute_track_posterior_probability_components(track);
	}
}


virtual_scan_track_set_t *
add_track(virtual_scan_track_set_t *track_set_n_1, virtual_scan_track_t *new_track, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	if (track_set_n_1 == NULL)
	{
		track_set_n_1 = (virtual_scan_track_set_t *) malloc(sizeof(virtual_scan_track_set_t));
		track_set_n_1->tracks = (virtual_scan_track_t **) malloc(sizeof(virtual_scan_track_t *));
		track_set_n_1->tracks[0] = new_track;
		track_set_n_1->size = 1;
		track_set_n_1->vertex_selected = (bool *) calloc (neighborhood_graph->size, sizeof(bool));
	}
	else
	{
		track_set_n_1->tracks = (virtual_scan_track_t **) realloc(track_set_n_1->tracks, (track_set_n_1->size + 1) * sizeof(virtual_scan_track_t *));
		track_set_n_1->tracks[track_set_n_1->size] = new_track;
		track_set_n_1->size += 1;
	}

	return (track_set_n_1);
}


virtual_scan_track_set_t *
track_birth(virtual_scan_track_set_t *track_set, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	if (neighborhood_graph != NULL)
	{
		int v_star_size;
		int *v_star = get_v_star(v_star_size, track_set, neighborhood_graph);
		if (v_star != NULL)
		{
			virtual_scan_track_t *new_track = (virtual_scan_track_t *) malloc(sizeof(virtual_scan_track_t));
			new_track->box_model_hypothesis = (virtual_scan_box_model_hypothesis_t *) malloc(sizeof(virtual_scan_box_model_hypothesis_t));
			new_track->size = 1;

			int rand_v = carmen_int_random(v_star_size);
			new_track->box_model_hypothesis[0] = *(neighborhood_graph->box_model_hypothesis[v_star[rand_v]]);
			track_set = add_track(track_set, new_track, neighborhood_graph);
			track_set->vertex_selected[v_star[rand_v]] = true;

			free(v_star);

			compute_hypothesis_posterior_probability_components(&(new_track->box_model_hypothesis[0]));

			return (track_set);
		}
		else
			return (track_set);
	}
	else
		return (NULL);
}


void
add_hypothesis_at_the_end(virtual_scan_track_set_t *track_set, int track_id, virtual_scan_box_model_hypothesis_t *hypothesis)
{
	if (hypothesis == NULL)
		return;

	virtual_scan_track_t *track = track_set->tracks[track_id];
	track->box_model_hypothesis = (virtual_scan_box_model_hypothesis_t *) realloc(track->box_model_hypothesis,
			(track->size + 1) * sizeof(virtual_scan_box_model_hypothesis_t));

	track->box_model_hypothesis[track->size] = *hypothesis;
	track->size += 1;

	track_set->vertex_selected[hypothesis->index] = true;
}


void
add_hypothesis_at_the_beginning(virtual_scan_track_set_t *track_set, int track_id, virtual_scan_box_model_hypothesis_t *hypothesis)
{
	if (hypothesis == NULL)
		return;

	virtual_scan_track_t *track = track_set->tracks[track_id];
	track->box_model_hypothesis = (virtual_scan_box_model_hypothesis_t *) realloc(track->box_model_hypothesis,
			(track->size + 1) * sizeof(virtual_scan_box_model_hypothesis_t));
	for (int i = track->size; i > 0; i--)
		track->box_model_hypothesis[i] = track->box_model_hypothesis[i - 1];

	track->box_model_hypothesis[0] = *hypothesis;
	track->size += 1;

	track_set->vertex_selected[hypothesis->index] = true;
}


bool
hypothesis_in_v_star(int hypothesis, int *v_star, int v_star_size)
{
	for (int i = 0; i < v_star_size; i++)
		if (v_star[i] == hypothesis)
			return (true);

	return (false);
}


int *
get_neighbors_within_v_star(int neighbor_type, int &number_of_neighbors, virtual_scan_box_model_hypothesis_edges_t *hypothesis_neighbors, int *v_star, int v_star_size)
{
	if (hypothesis_neighbors == NULL)
		return (NULL);

	number_of_neighbors = 0;
	for (int i = 0; i < hypothesis_neighbors->size; i++)
	{
		if ((hypothesis_neighbors->edge_type[i] == neighbor_type) && hypothesis_in_v_star(hypothesis_neighbors->edge[i], v_star, v_star_size))
			number_of_neighbors++;
	}

	if (number_of_neighbors != 0)
	{
		int *neighbors = (int *) malloc(number_of_neighbors * sizeof(int));
		for (int i = 0, j = 0; i < hypothesis_neighbors->size; i++)
		{
			if ((hypothesis_neighbors->edge_type[i] == neighbor_type) && hypothesis_in_v_star(hypothesis_neighbors->edge[i], v_star, v_star_size))
			{
				neighbors[j] = hypothesis_neighbors->edge[i];
				j++;
			}
		}
		return (neighbors);
	}
	else
		return (NULL);
}


virtual_scan_box_model_hypothesis_t *
get_child_hypothesis_in_v_star_and_at_the_end_of_track(virtual_scan_track_t *track, virtual_scan_neighborhood_graph_t *neighborhood_graph,
		int *v_star, int v_star_size)
{
	virtual_scan_box_model_hypothesis_t end_of_track = track->box_model_hypothesis[track->size - 1];
	virtual_scan_box_model_hypothesis_edges_t *hypothesis_neighbors = neighborhood_graph->box_model_hypothesis_edges[end_of_track.index];
	int number_of_children;
	int *hypothesis_children = get_neighbors_within_v_star(CHILD_EDGE, number_of_children, hypothesis_neighbors, v_star, v_star_size);
	if (hypothesis_children != NULL)
	{
		int rand_hypothesis = carmen_int_random(number_of_children);
		virtual_scan_box_model_hypothesis_t *child_hypothesis = neighborhood_graph->box_model_hypothesis[hypothesis_children[rand_hypothesis]];
		free(hypothesis_children);

		return (child_hypothesis);
	}
	else
		return (NULL);
}


virtual_scan_box_model_hypothesis_t *
get_parent_hypothesis_in_v_star_and_at_the_beginning_of_track(virtual_scan_track_t *track, virtual_scan_neighborhood_graph_t *neighborhood_graph,
		int *v_star, int v_star_size)
{
	virtual_scan_box_model_hypothesis_t beginning_of_track = track->box_model_hypothesis[0];
	virtual_scan_box_model_hypothesis_edges_t *hypothesis_neighbors = neighborhood_graph->box_model_hypothesis_edges[beginning_of_track.index];
	int number_of_parents;
	int *hypothesis_parents = get_neighbors_within_v_star(PARENT_EDGE, number_of_parents, hypothesis_neighbors, v_star, v_star_size);
	if (hypothesis_parents != NULL)
	{
		int rand_hypothesis = carmen_int_random(number_of_parents);
		virtual_scan_box_model_hypothesis_t *parent_hypothesis = neighborhood_graph->box_model_hypothesis[hypothesis_parents[rand_hypothesis]];
		free(hypothesis_parents);

		return (parent_hypothesis);
	}
	else
		return (NULL);
}


void
track_extension(virtual_scan_track_set_t *track_set, int track_id, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	if (track_set == NULL)
		return;

	virtual_scan_track_t *track = track_set->tracks[track_id];
	virtual_scan_box_model_hypothesis_t *hypothesis;
	do
	{
		int v_star_size;
		int *v_star = get_v_star(v_star_size, track_set, neighborhood_graph);
		if (v_star != NULL)
		{
			if (carmen_int_random(2) == 0)
			{
				hypothesis = get_child_hypothesis_in_v_star_and_at_the_end_of_track(track, neighborhood_graph, v_star, v_star_size);
				compute_hypothesis_posterior_probability_components(hypothesis);

				add_hypothesis_at_the_end(track_set, track_id, hypothesis);
			}
			else
			{
				hypothesis = get_parent_hypothesis_in_v_star_and_at_the_beginning_of_track(track, neighborhood_graph, v_star, v_star_size);
				compute_hypothesis_posterior_probability_components(hypothesis);

				add_hypothesis_at_the_beginning(track_set, track_id, hypothesis);
			}
			free(v_star);
//			if (neighborhood_graph->graph_id == 7)
//			{
//				print_track_set(track_set, neighborhood_graph, 777);
//				printf("pare\n");
//			}
		}
		else
			break;
	} while (carmen_uniform_random(0.0, 1.0) > GAMMA);
}


void
track_reduction(virtual_scan_track_set_t *track_set, int track_id)
{
	virtual_scan_track_t *track = track_set->tracks[track_id];
	int previous_size = track->size;

	if (track->size > 2)
	{
		int r = carmen_int_random(track->size - 2) + 1; // r entre 1 e (track->size - 2). Note que, diferente do paper, nossa numeracao comecca de 0
		if (carmen_int_random(2) == 0)
		{	// forward reduction
			track->size = r + 1;

			for (int i = track->size; i < previous_size; i++)
				track_set->vertex_selected[track->box_model_hypothesis[i].index] = false;
		}
		else
		{	// backward reduction
			track->size -= r;

			for (int i = 0; i < r; i++)
				track_set->vertex_selected[track->box_model_hypothesis[i].index] = false;

			memmove((void *) &(track->box_model_hypothesis[0]), (void *) &(track->box_model_hypothesis[r]),
					track->size * sizeof(virtual_scan_box_model_hypothesis_t));
		}

		track->box_model_hypothesis = (virtual_scan_box_model_hypothesis_t *) realloc(track->box_model_hypothesis,
				track->size * sizeof(virtual_scan_box_model_hypothesis_t));
	}
}


virtual_scan_track_set_t *
track_death(virtual_scan_track_set_t *track_set, int victim)
{
	for (int i = 0; i < track_set->tracks[victim]->size; i++)
		track_set->vertex_selected[track_set->tracks[victim]->box_model_hypothesis[i].index] = false;

	if (track_set->size > 1)
	{
		free_track(track_set->tracks[victim]);
		memmove((void *) &(track_set->tracks[victim]), &(track_set->tracks[victim + 1]), (track_set->size - (victim + 1)) * sizeof(virtual_scan_track_t *)); // verificar (victim + 1)
		track_set->size -= 1; // realloc
	}
	else
	{
		free_track_set(track_set);
		track_set = NULL;
	}

	return (track_set);
}


//void
//track_split(virtual_scan_track_t *track)
//{
//
//}
//
//
//void
//track_merge(virtual_scan_track_set_t *track_set, int rand_track, int rand_track2)
//{
//
//}
//
//
//void
//track_diffusion(virtual_scan_track_t *track)
//{
//
//}


virtual_scan_track_set_t *
copy_track_set(virtual_scan_track_set_t *track_set_n_1, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	if (track_set_n_1 != NULL)
	{
		virtual_scan_track_set_t *track_set = (virtual_scan_track_set_t *) malloc(sizeof(virtual_scan_track_set_t));
		track_set->tracks = (virtual_scan_track_t **) malloc(track_set_n_1->size * sizeof(virtual_scan_track_t *));

		track_set->size = track_set_n_1->size;
		for (int i = 0; i < track_set_n_1->size; i++)
		{
			track_set->tracks[i] = (virtual_scan_track_t *) malloc(sizeof(virtual_scan_track_t));
			track_set->tracks[i]->size = track_set_n_1->tracks[i]->size;
			track_set->tracks[i]->box_model_hypothesis =
					(virtual_scan_box_model_hypothesis_t *) malloc(track_set_n_1->tracks[i]->size * sizeof(virtual_scan_box_model_hypothesis_t));
			for (int j = 0; j < track_set_n_1->tracks[i]->size; j++)
				track_set->tracks[i]->box_model_hypothesis[j] = track_set_n_1->tracks[i]->box_model_hypothesis[j];
		}

		track_set->vertex_selected = (bool *) malloc(neighborhood_graph->size * sizeof(bool));
		memcpy((void *) track_set->vertex_selected, (void *) track_set_n_1->vertex_selected, neighborhood_graph->size * sizeof(bool));

		return (track_set);
	}
	else
		return (NULL);
}


bool
stop_condition(virtual_scan_track_set_t *track_set, virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	if (track_set == NULL)
		return (false);

	int num_hypothesis = 0;
	for (int i = 0; i < track_set->size; i++)
		num_hypothesis += track_set->tracks[i]->size;

	int num_taken_vertexes = 0;
	for (int i = 0; i < neighborhood_graph->size; i++)
		num_taken_vertexes += (track_set->vertex_selected[i] == 0)? 0: 1;

	if (num_hypothesis != num_taken_vertexes)
		return (true);
	else
		return (false);
}


virtual_scan_track_set_t *
propose_track_set_according_to_q(virtual_scan_neighborhood_graph_t *neighborhood_graph, virtual_scan_track_set_t *track_set_n_1)
{
#define NUMBER_OF_TYPES_OF_MOVES	4

//	static int num_proposal = 0;

	int rand_track;

	int rand_move = carmen_int_random(NUMBER_OF_TYPES_OF_MOVES);
	if ((track_set_n_1 != NULL) && (track_set_n_1->size != 0))
		rand_track = carmen_int_random(track_set_n_1->size);
	else
		rand_track = -1;

	virtual_scan_track_set_t *track_set = copy_track_set(track_set_n_1, neighborhood_graph);

//	print_track_set(track_set, neighborhood_graph, num_proposal);
//	if (track_set != NULL)
//	{
//		FILE *track_sets = fopen("track_sets.txt", "a");
//		fprintf(track_sets, "rand_move %d, rand_track %d\n", rand_move, rand_track);
//		fclose(track_sets);
//	}

	switch (rand_move)
	{
		case 0:	// Birth
			track_set = track_birth(track_set, neighborhood_graph);
			if (track_set != NULL)
				track_extension(track_set, track_set->size - 1, neighborhood_graph);
			break;
		case 1:	// Extension
			if (rand_track != -1)
				track_extension(track_set, rand_track, neighborhood_graph);
			break;
		case 2:	// Reduction
			if (rand_track != -1)
				track_reduction(track_set, rand_track);
			break;
		case 3: // Death
			if (rand_track != -1)
				track_set = track_death(track_set, rand_track);
			break;
//		case 4:	// Split
//			if (rand_track != -1)
//			{
//				if (track_set->tracks[rand_track].size >= 4)
//					track_split(&(track_set->tracks[rand_track]));
//			}
//			break;
//		case 5:	// Merge
//			if (rand_track != -1)
//			{
//				int rand_track2 = carmen_int_random(track_set->size);
//				track_merge(track_set, rand_track, rand_track2);
//			}
//			break;
//		case 6:	// Diffusion
//			if (rand_track != -1)
//				track_diffusion(&track_set->tracks[rand_track]);
//			break;
	}

//	print_track_set(track_set, neighborhood_graph, num_proposal++);
	if (stop_condition(track_set, neighborhood_graph))
		printf("stop condition\n");

	return (track_set);
}


void
virtual_scan_free_moving_objects(carmen_moving_objects_point_clouds_message *moving_objects)
{
	if (moving_objects != NULL)
	{
		free(moving_objects->point_clouds);
		free(moving_objects);
	}
}


double
A(virtual_scan_track_set_t *track_set_n, virtual_scan_track_set_t *track_set_prime)
{
	double pi_w_prime = probability_of_track_set_given_measurements(track_set_prime);
	double pi_w_n_1 = probability_of_track_set_given_measurements(track_set_n);
	double q_w_w_prime = 1.0 / NUMBER_OF_TYPES_OF_MOVES;
	double q_w_prime_w = 1.0 / NUMBER_OF_TYPES_OF_MOVES;

	if (pi_w_prime == 0.0)
		return (0.0);
	else if (pi_w_n_1 == 0.0)
		return (1.0);

	double A_w_w_prime = carmen_fmin(1.0, (pi_w_prime * q_w_w_prime) / (pi_w_n_1 * q_w_prime_w));

	return (A_w_w_prime);
}


virtual_scan_track_set_t *
virtual_scan_infer_moving_objects(virtual_scan_neighborhood_graph_t *neighborhood_graph)
{
	virtual_scan_track_set_t *track_set_n = copy_track_set(best_track_set, neighborhood_graph);
	for (int n = 0; n < MCMC_MAX_ITERATIONS; n++)
	{
//		if (neighborhood_graph->graph_id == 7)
//		{
//			print_track_set(track_set_n, neighborhood_graph, 999);
//			printf("pare\n");
//		}
		virtual_scan_track_set_t *track_set_prime = propose_track_set_according_to_q(neighborhood_graph, track_set_n);
		virtual_scan_track_set_t *track_set_victim = track_set_prime;
		double U = carmen_uniform_random(0.0, 1.0);
		if (U < A(track_set_n, track_set_prime))
		{
			track_set_victim = track_set_n;
			track_set_n = track_set_prime;
			if (probability_of_track_set_given_measurements(track_set_n) > probability_of_track_set_given_measurements(best_track_set))
			{
				free_track_set(best_track_set);
				best_track_set = copy_track_set(track_set_n, neighborhood_graph);
			}
		}
		free_track_set(track_set_victim);
	}
	free_track_set(track_set_n);

	double best_track_prob = probability_of_track_set_given_measurements(best_track_set);
	printf("num_tracks %d, prob %lf\n", (best_track_set)? best_track_set->size: 0, best_track_prob);
//	print_track_set(best_track_set, neighborhood_graph, 0);

	return (best_track_set);
}
