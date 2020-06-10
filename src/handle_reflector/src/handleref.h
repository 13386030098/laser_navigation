#include <ros/ros.h>
#include <sensor_msgs/LaserScan.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/PoseStamped.h>
#include <tf/transform_broadcaster.h>
#include <tf/transform_listener.h>
#include <iostream>
#include <vector>

#include <cmath>
#include <ctime>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/StdVector>

class HandleRef
{
public:
  ros::NodeHandle n;
  HandleRef();
  virtual ~HandleRef(){}

  bool solveLeastSquaresCircleKasa(const  std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d> > &points, Eigen::Vector2d &midpoint, double &radius);

  float getDis(float x1, float y1, float x2, float y2);
  bool findcircles(std::vector<float> num, int count, std::vector<Eigen::Matrix2d>& info);
//  void findPeaks(std::vector<float> num, int count);
  void matching_ref( );

  //TF Scalar Listener
  tf::TransformListener transform_listener;
  tf::StampedTransform transform;

  void get_reflocation(float x, float y);

  /*Callback Functions*/
  void scan_Callback(const sensor_msgs::LaserScan::ConstPtr &msg);

  ros::Subscriber scan_sub;


  /*Public Functions*/
  ros::Publisher robot_pub;

  //测试用pub
  ros::Publisher test_point_pub;

};
