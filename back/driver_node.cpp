#include "ros/ros.h"
#include <sstream>
#include <stdio.h>
#include <iostream>
#include "controlcan.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include "unistd.h"

#include <robot_msgs/odometer.h>
#include <geometry_msgs/Pose2D.h>

#include <thread>
#include <Eigen/Eigen>

#define pi 3.141593

class control_driver
{
private:
  short speed;
  short angle;
  short current;
  short direction;
  short power;
  short mode;

  double wheel_base;
  double delta_t;

  ros::NodeHandle nh_;
  ros::Publisher  odometer_pub_;
  ros::Subscriber path_sub_;
  ros::Subscriber localization_sub_;

  std::thread* vehicle_status_thread_;
  std::thread* control_thread_;
  std::thread* odometer_thread_;

  std::vector<double>  world_pos_x;
  std::vector<double>  world_pos_y;
  std::vector<geometry_msgs::Pose2D>  interpolation_pos_list;
  geometry_msgs::Pose2D pos_start, pos_end;
  geometry_msgs::Pose2D localization_pos;
  int pos_list_k;
  bool init_pos;

  bool linear_interpolation;
  bool cubic_interpolation;
  bool quintic_interpolation;

public:
  control_driver():
  init_pos(true),
  pos_list_k(0),
  linear_interpolation(true),
  cubic_interpolation(false),
  quintic_interpolation(false)
  {
    odometer_pub_ = nh_.advertise<robot_msgs::odometer>("/odometer", 100, true);
    path_sub_ = nh_.subscribe("/path_point_", 100, &control_driver::one_path_point_, this);
//    localization_sub_ = nh_.subscribe("/localization_", 100, &control_driver::localization_, this);

//    vehicle_status_thread_ = new std::thread(boost::bind(&control_driver::vehicle_status_func,this));// read footswitch thread
    control_thread_ = new std::thread(boost::bind(&control_driver::control_func,this));// control thread
    odometer_thread_ = new std::thread(boost::bind(&control_driver::odometer_func,this));// control thread
  }

  ~control_driver(){}

  void one_path_point_(const geometry_msgs::Pose2DPtr& path_point_msg)
  {
    if(init_pos){
      world_pos_x.push_back(0);//need to change
      world_pos_y.push_back(0);
      init_pos = false;
    }
    world_pos_x.push_back(path_point_msg->x);
    world_pos_y.push_back(path_point_msg->y);

    if(world_pos_x.size() >1){
      interpolation_pos_list.clear();

      pos_start.x = world_pos_x[pos_list_k];
      pos_start.y = world_pos_y[pos_list_k];

      pos_end.x = world_pos_x[pos_list_k+1];
      pos_end.y = world_pos_y[pos_list_k+1];

      pos_list_k ++;

      //straight line interpolation
      if(linear_interpolation){
        for(int i=0; i<5 ; i++){
          geometry_msgs::Pose2D pos_temp;
          pos_temp.x = pos_start.x + i*(pos_end.x - pos_start.x)/5;
          pos_temp.y = pos_start.y + i*(pos_end.y - pos_start.y)/5;
          interpolation_pos_list.push_back(pos_temp);
        }
      }
//      if(cubic_interpolation){
//      }

//      if(quintic_interpolation){
//      }

      interpolation_pos_list.push_back(pos_end);
    }

  }

  void control_func(){

    //  if(VCI_OpenDevice(VCI_USBCAN1,0,0)==1)
    //  {
    //    printf(">>open deivce success!\n");
    //  }
    //  else
    //  {
    //    printf(">>open deivce error!\n");
    //    exit(1);
    //  }
    //  //init CAN Configuration
    //  VCI_INIT_CONFIG config;
    //  config.AccCode=0;
    //  config.AccMask=0xFFFFFFFF;
    //  config.Filter=1;
    //  config.Timing0=0x03;/*125 Kbps  Timing0=0x03 Timing1= 0x1C*/
    //  config.Timing1=0x1C;
    //  config.Mode=0;

    //  if(VCI_InitCAN(VCI_USBCAN1,0,0,&config)!=1)//CAN1
    //  {
    //    printf(">>Init CAN1 error\n");
    //    VCI_CloseDevice(VCI_USBCAN1,0);
    //  }

    //  if(VCI_StartCAN(VCI_USBCAN1,0,0)!=1)
    //  {
    //    printf(">>Start CAN1 error\n");
    //    VCI_CloseDevice(VCI_USBCAN1,0);
    //  }

    ros::Rate loop_rate(5);//0.2s
    while (ros::ok()) {
//    ros::Time begin = ros::Time::now();
      double nearest_point_distance = 100;
      double nearest_point_distance_temp = 0;
      geometry_msgs::Pose2D goal_point_in_world_frame;
      geometry_msgs::Pose2D goal_point_in_vehicle_frame;
      int nearest_point_index;
      for(int i=0; i<6; i++)
      {
        nearest_point_distance_temp = hypot(fabs(localization_pos.x - interpolation_pos_list[i].x),
                                            fabs(localization_pos.y - interpolation_pos_list[i].y));
        if(nearest_point_distance_temp < nearest_point_distance){
          nearest_point_distance = nearest_point_distance_temp;
          nearest_point_index = i;
        }
      }

      if(nearest_point_index < 5){
        goal_point_in_world_frame = interpolation_pos_list[nearest_point_index + 1];
        //change to self frame
        double delta_x = goal_point_in_vehicle_frame.x;
        double delta_y = goal_point_in_vehicle_frame.y;
        double delta_theta = atan2(delta_y, delta_x);
      }

//    walking_motor(0, 0x02);
//    steering_motor(4000);

    ros::spinOnce();
    loop_rate.sleep();

//    ros::Time end = ros::Time::now();
//    std::cout << end - begin <<std::endl;

    }
//    VCI_CloseDevice(VCI_USBCAN1, 0);
  }

  Eigen::Matrix<double,2,1> frame_world_to_vehicle_func(Eigen::Matrix<double,2,1> desire_world_pos,
                                                        Eigen::Matrix<double,2,1> actual_world_pos,
                                                        double world_orientation)
  {
    Eigen::Matrix<double,2,1> pos_in_vehicle_frame_express_in_world = desire_world_pos - actual_world_pos;
    Eigen::Matrix<double,2,2> rotation_world_to_vehicle;

    rotation_world_to_vehicle<<
        cos(world_orientation), -sin(world_orientation),
        sin(world_orientation),  cos(world_orientation);

    Eigen::Matrix<double,2,1> pos_in_vehicle_frame = rotation_world_to_vehicle.inverse() * pos_in_vehicle_frame_express_in_world;
    return pos_in_vehicle_frame;
  }

  void odometer_func()
  {
    int j;
    int ind=0;
    int reclen=0;
    VCI_CAN_OBJ rec[3000];

    ros::Rate loop_rate(100);//delt = 0.01s
    while (ros::ok())
    {
      Eigen::Matrix<double,3,1> old_odometer;
      Eigen::Matrix<double,3,1> now_odometer;
      old_odometer << 0,0,0;//need to change, x, y, alfa

      if((reclen=VCI_Receive(VCI_USBCAN1,0,ind,rec,3000,100))>0){
        for(j=0;j<reclen;j++){
          if(rec[j].ID==0x1A6){
            speed=(rec[j].Data[0]<<8)|rec[j].Data[1];
//            std::cout << speed <<std::endl;
          }
          if(rec[j].ID==0x2A6){
            direction = rec[j].Data[0] & 0x01;
            angle=(rec[j].Data[2]<<8)|rec[j].Data[3];
//            std::cout << angle <<std::endl;
          }
        }
      }
      if(direction){
        speed = pi/30 * speed;
      }else{
        speed = -pi/30 * speed;
      }
      angle = pi/180 * angle;

      now_odometer(0,0) = old_odometer(0,0) - wheel_base/tan(angle) * sin(old_odometer(2,0)) +
          wheel_base/tan(angle) * sin(old_odometer(2,0) + (speed * delta_t * sin(angle))/wheel_base);
      now_odometer(1,0) = old_odometer(1,0) + wheel_base/tan(angle) * cos(old_odometer(2,0)) -
          wheel_base/tan(angle) * cos(old_odometer(2,0) + (speed * delta_t * sin(angle))/wheel_base);
      now_odometer(2,0) = old_odometer(2,0) + (speed * delta_t * sin(angle))/wheel_base;

      robot_msgs::odometer odometer_msg;
      odometer_msg.data.resize(3);
      odometer_msg.data[0] = now_odometer(0,0);
      odometer_msg.data[1] = now_odometer(1,0);
      odometer_msg.data[2] = now_odometer(2,0);
      odometer_pub_.publish(odometer_msg);

      ros::spinOnce();
      loop_rate.sleep();
    }

  }
  /*****************can driver********************/
  //velocity,0-8000对应0-8000rpm/min
  //direction:forward,backward
  //direction 0x01-forward,0x02-backward
  void walking_motor(short velocity,short direction)
  {
      VCI_CAN_OBJ send[1];
      send[0].ID=0x226;//0x200+Tra-ID(0x26)
      send[0].SendType=0;
      send[0].RemoteFlag=0;
      send[0].ExternFlag=0;
      send[0].DataLen=8;

      if(velocity>8000)velocity=8000;
      if(velocity<0)velocity=0;

      send[0].Data[0] =0x04+direction;
      send[0].Data[1] =(unsigned char)((velocity>>8)&0xff);
      send[0].Data[2] =(unsigned char)(velocity&0xff);
      send[0].Data[3] =10;
      send[0].Data[4] =10;
      send[0].Data[5] =10;
      send[0].Data[6] =10;

      VCI_Transmit(VCI_USBCAN1,0,0,send,1);
  }

  //angle:-9000~+9000,对应（-90deg～90deg）,-9000左转,+9000右转
  void steering_motor(short angle)
  {
      VCI_CAN_OBJ send[1];
      send[0].ID=0x326;//0x300+Tra-ID
      send[0].SendType=0;
      send[0].RemoteFlag=0;
      send[0].ExternFlag=0;
      send[0].DataLen=8;

      if(angle>9000)angle=9000;
      if(angle<-9000)angle=-9000;

      send[0].Data[0] =0;
      send[0].Data[1] =(unsigned char)((angle>>8)&0xff);
      send[0].Data[2] =(unsigned char)(angle&0xff);
      VCI_Transmit(VCI_USBCAN1,0,0,send,1);
  }

  //0x01,up
  //0x02,down
  void up_down(short up_down)
  {
    VCI_CAN_OBJ send[1];
      send[0].ID=0x326;//0x300+Tra-ID
      send[0].SendType=0;
      send[0].RemoteFlag=0;
      send[0].ExternFlag=0;
      send[0].DataLen=8;

      send[0].Data[0] =up_down;
      VCI_Transmit(VCI_USBCAN1,0,0,send,1);
  }
  /*****************can driver********************/


  /*****************status reflection********************/
  void vehicle_status_func(void){
    int j;
    int ind=0;
    int reclen=0;
    VCI_CAN_OBJ rec[3000];

    if((reclen=VCI_Receive(VCI_USBCAN1,0,ind,rec,3000,100))>0){
      for(j=0;j<reclen;j++){
        if(rec[j].ID==0x1A6){
          speed=(rec[j].Data[0]<<8)|rec[j].Data[1];
        }
        if(rec[j].ID==0x2A6){
          direction = rec[j].Data[0] & 0x01;
//          mode = rec[j].Data[0] & 0x10;
          angle=(rec[j].Data[2]<<8)|rec[j].Data[3];
          std::cout << "angle: " <<angle << " deg" <<std::endl;
          power = rec[j].Data[1];
          if(power < 20){
            std::cout << "power: " <<power << "%" <<std::endl;
            ROS_ERROR("Low battery, please charge");
            exit(-1);
          }
          else{
            std::cout << "power: " <<power << "%" <<std::endl;
          }
        }
        if(direction ==1 ){
          speed = speed;
        }
        else{
          speed = -speed;
        }

        std::cout << "speed: " <<speed << " rpm/min" <<std::endl;

      }
    }
  }
  /*****************status reflection********************/

  /*****************thread waiting********************/
  void halt(void)
  {
    control_thread_ -> join();
//    odometer_thread_ -> join();
  }
  /*****************thread waiting********************/

  void start(void)
  {
    std::cout << "vehicle initialization ..." <<std::endl;
  }

};

int main(int argc, char** argv)
{
  std::cout << "ok" <<std::endl;

  ros::init(argc, argv, "laser");
  control_driver driver_node;


  std::cout << "ok" <<std::endl;
  driver_node.start();
//  driver_node.vehicle_status_func();
  std::cout << "ok" <<std::endl;
  ros::AsyncSpinner spinner(4);
  spinner.start();
  driver_node.halt();
  ros::waitForShutdown();
  return 0;
}






























































