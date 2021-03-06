/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  num_particles = 100;
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of 
	//   x, y, theta and their uncertainties from GPS) and all weights to 1. 
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).
  default_random_engine gen;
  normal_distribution<double> dist_x(x, std[0]);
  normal_distribution<double> dist_y(y, std[1]);
  normal_distribution<double> dist_theta(theta, std[2]);
  double initial_weight = 1.0;
  
  for(int i = 0; i < num_particles; i++) {
    Particle p;
    p.id = i;
    p.x = dist_x(gen);
    p.y = dist_y(gen);
    p.theta = dist_theta(gen);
    p.weight = initial_weight;
    particles.push_back(p);
    weights.push_back(initial_weight);
  }
  is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distributio
	//  http://www.cplusplus.com/reference/random/default_random_engine/
  default_random_engine gen;
  Particle *p;
  for(int i = 0; i < num_particles; i++){
    p = &particles[i];
    double velocity_yaw_rate_prop = velocity / yaw_rate;
    double yaw_delta_t_product = yaw_rate * delta_t;
    
    if(fabs(yaw_rate) > 0.001){
      p->x += velocity_yaw_rate_prop * (sin(p->theta + yaw_delta_t_product) - sin(p->theta));
      p->y += velocity_yaw_rate_prop * (cos(p->theta) - cos(p->theta + yaw_delta_t_product));
      p->theta += yaw_delta_t_product;
    } else {
      p->x = p->x + velocity * delta_t * cos(p->theta);
      p->y = p->y + velocity * delta_t * sin(p->theta);
      p->theta = p->theta;
    }
    normal_distribution<double> dist_x(p->x, std_pos[0]);
    normal_distribution<double> dist_y(p->y, std_pos[1]);
    normal_distribution<double> dist_theta(p->theta, std_pos[2]);
    
    p->x = dist_x(gen);
    p->y = dist_y(gen);
    p->theta = dist_theta(gen);
  }
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the 
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
	//   implement this method and use it as a helper during the updateWeights phase.
  for(int i = 0; i < observations.size(); i++){
    LandmarkObs* o = &observations[i];
    double curr_distance = INFINITY;
    int closest_id = 0;
    for(int j = 0; j < predicted.size(); j++){
      LandmarkObs p = predicted[j];
      double distance = dist(o->x, o->y, p.x, p.y);
      if(distance < curr_distance){
        curr_distance = distance;
        closest_id = p.id;
      }
    }
    o->id = closest_id;
  }

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
		std::vector<LandmarkObs> observations, Map map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation 
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html
  
  double std_x = std_landmark[0];
  double std_y = std_landmark[1];
  for (int i = 0; i < num_particles; i++){
    std::vector<LandmarkObs> transformed_observations;
    Particle* p = &particles[i];
    for(int j = 0; j < observations.size(); j++){
      LandmarkObs t_ob;
      LandmarkObs c_obs = observations[j];
      t_ob.x = p->x + c_obs.x * cos(p->theta) - c_obs.y * sin(p->theta);
      t_ob.y = p->y + c_obs.x * sin(p->theta) + c_obs.y * cos(p->theta);
      t_ob.id = c_obs.id;
      transformed_observations.push_back(t_ob);
    }
    
    std::vector<LandmarkObs> landmarks_in_range;
    for (int j = 0; j < map_landmarks.landmark_list.size(); j++){
      Map::single_landmark_s landmark = map_landmarks.landmark_list[j];
      double distance  = dist(p->x, p->y, landmark.x_f, landmark.y_f);
      if (distance < sensor_range) {
        LandmarkObs landmark_obj;
        landmark_obj.x = landmark.x_f;
        landmark_obj.y = landmark.y_f;
        landmark_obj.id = landmark.id_i;
        landmarks_in_range.push_back(landmark_obj);
      }
    }
    
    dataAssociation(landmarks_in_range, transformed_observations);
    
    double weight = 1.0;
    for(int j = 0; j < transformed_observations.size(); j++){
      LandmarkObs t_ob = transformed_observations[j];
      LandmarkObs l;
      for(int k = 0; k < landmarks_in_range.size(); k++){
        if (landmarks_in_range[k].id == t_ob.id){
          l = landmarks_in_range[k];
        }
      }
      double gaussian_p = (1/(2.0 * M_PI * std_x * std_y)) * exp(-(pow(t_ob.x - l.x, 2) / (2 * pow(std_x, 2)) + pow(t_ob.y - l.y, 2) / (2 * pow(std_y, 2))));

      weight *= gaussian_p;
    }
    p->weight = weight;
    weights[i] = weight;
  }
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight.
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
  default_random_engine gen;
  discrete_distribution<int> weight_dist(weights.begin(), weights.end());
  std::vector<Particle> resampled_particles;
  for(int i = 0; i < num_particles; i++) {
    resampled_particles.push_back(particles[weight_dist(gen)]);
  }
  particles = resampled_particles;
}

Particle ParticleFilter::SetAssociations(Particle particle, std::vector<int> associations, std::vector<double> sense_x, std::vector<double> sense_y)
{
	//particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
	// associations: The landmark id that goes along with each listed association
	// sense_x: the associations x mapping already converted to world coordinates
	// sense_y: the associations y mapping already converted to world coordinates

	//Clear the previous associations
	particle.associations.clear();
	particle.sense_x.clear();
	particle.sense_y.clear();

	particle.associations= associations;
 	particle.sense_x = sense_x;
 	particle.sense_y = sense_y;

 	return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
