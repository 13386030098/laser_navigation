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

#include <tf/transform_listener.h>
#include <robot_msgs/odometer.h>
#include <geometry_msgs/Pose2D.h>

#include <thread>
#include <Eigen/Eigen>

#define pi 3.141593

class control_driver
{
private:
  short speed;
  double speed_double;
  short angle;
  double angle_double;
  short current;
  short power;
  short mode;

  double wheel_base;
  double delta_t;
  double ratio;

  ros::NodeHandle nh_;
  ros::Publisher  odometer_pub_;
  ros::Subscriber cmd_vel_sub_;
  std::thread* control_thread_;
  std::thread* odometer_thread_;
  std::thread* interpolation_thread_;
  
  std::stringstream cmdss;

  std::vector<double>  world_pos_x;
  std::vector<double>  world_pos_y;
  std::vector<geometry_msgs::PoseStamped>  interpolation_pos_list;
  geometry_msgs::PoseStamped pos_start, pos_end;
  tf::TransformListener listener;

  int pos_list_k;
  bool init_pos;
  bool symbol;

  bool linear_interpolation;

  Eigen::Matrix<double,3,1> old_odometer;
  Eigen::Matrix<double,3,1> now_odometer;

  Eigen::Matrix<double,2,1> world_pos_command_x_y;//x,y,total 4 points

public:
  control_driver():
   init_pos(true),
   pos_list_k(0),
   linear_interpolation(true),
   ratio(28),
   wheel_base(1.415),
   delta_t(0.01),
   old_odometer(0,0,0),
   world_pos_command_x_y(2,1),
   listener(ros::Duration(10)),
   symbol(false)
  {


    odometer_pub_ = nh_.advertise<robot_msgs::odometer>("/odometer", 100, true);
    cmd_vel_sub_ = nh_.subscribe("/cmd_vel", 1000,&control_driver::chatterCallback,this);
    //control_thread_ = new std::thread(boost::bind(&control_driver::control_func,this));// control thread
    odometer_thread_ = new std::thread(boost::bind(&control_driver::odometer_func,this));// odometer thread
    //interpolation_thread_ = new std::thread(boost::bind(&control_driver::interpolation_func,this));// odometer thread

init_CAN();
  }
  void chatterCallback(const geometry_msgs::Twist& msg)
{ 
  double v=msg.linear.x;

  double angle = msg.angular.z;
  if(v>0)
  {
    walking_motor((short)(v),0x02);

  }
  else
  {
    walking_motor((short)(-v),0x01);
  }
  steering_motor((short)angle);
  // need change to real velocity
}
  void init_CAN()
  {
    if(VCI_OpenDevice(VCI_USBCAN1,0,0)==1)
      {
        printf(">>open deivce success!\n");
      }
      else
      {
        printf(">>open deivce error!\n");
        exit(-1);
      }
      //init CAN Configuration
      VCI_INIT_CONFIG config;
      config.AccCode=0;
      config.AccMask=0xFFFFFFFF;
      config.Filter=1;
      config.Timing0=0x03;/*125 Kbps  Timing0=0x03 Timing1= 0x1C*/
      config.Timing1=0x1C;
      config.Mode=0;

      if(VCI_InitCAN(VCI_USBCAN1,0,0,&config)!=1)//CAN1
      {
        printf(">>Init CAN1 error\n");
        VCI_CloseDevice(VCI_USBCAN1,0);
      }

      if(VCI_StartCAN(VCI_USBCAN1,0,0)!=1)
      {
        printf(">>Start CAN1 error\n");
        VCI_CloseDevice(VCI_USBCAN1,0);
      }

  }

  ~control_driver(){VCI_CloseDevice(VCI_USBCAN1, 0);}


  /*****************can driver********************/
  //velocity,0-8000对应0-8000rpm/min
  //direction:forward,backward
  //direction
  //0x02-forward
  //0x01-backward
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

  void odometer_func()
  {
    int j;
    int ind=0;
    int reclen=0;
    VCI_CAN_OBJ rec[3000];
    bool forward, backward;

    ros::Rate loop_rate(100);//delt = 0.01s
    while (ros::ok())
    {
      /*old_odometer << 0,0,0;*///need to change, x, y, alfa

      if((reclen=VCI_Receive(VCI_USBCAN1,0,ind,rec,3000,100))>0){
        for(j=0;j<reclen;j++){
          if(rec[j].ID==0x1A6){
            speed=(rec[j].Data[0]<<8)|rec[j].Data[1];
//            std::cout << speed <<std::endl;
          }
          if(rec[j].ID==0x2A6){
            forward  = rec[j].Data[0] & 0x01;
            backward = rec[j].Data[0] & 0x02;
            angle=(rec[j].Data[2]<<8)|rec[j].Data[3];
          }
        }
      }
//      std::cout << speed <<std::endl;

      if(forward == 1){
        speed_double = pi/30 * speed /ratio * 0.12;
      }else if(backward ==1){
        speed_double = -pi/30 * speed /ratio * 0.12;
      }
//      std::cout << angle <<std::endl;

//      angle_double = angle;
      angle_double = -pi/180 * (angle/100);
//      std::cout << angle_double <<std::endl;

//      std::cout << speed_double <<std::endl;
      double agv_Linear_velocity  = speed_double*cos(angle_double);
      double agv_angular_velocity = speed_double*sin(angle_double)/wheel_base;

      now_odometer(2,0) = old_odometer(2,0) + agv_angular_velocity*delta_t;//theta

      now_odometer(0,0) = old_odometer(0,0) + agv_Linear_velocity*cos(now_odometer(2,0))*delta_t;//x
      now_odometer(1,0) = old_odometer(1,0) + agv_Linear_velocity*sin(now_odometer(2,0))*delta_t;//y

      old_odometer = now_odometer;

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

  void vehicle_status_func(void){
    int j;
    int ind=0;
    int reclen=0;
    VCI_CAN_OBJ rec[3000];
    bool forward, backward;

    if((reclen=VCI_Receive(VCI_USBCAN1,0,ind,rec,3000,100))>0){
      for(j=0;j<reclen;j++){
        if(rec[j].ID==0x1A6){
          speed=(rec[j].Data[0]<<8)|rec[j].Data[1];
        }
        if(rec[j].ID==0x2A6){
          forward  = rec[j].Data[0] & 0x01;
          backward = rec[j].Data[0] & 0x02;
//          mode = rec[j].Data[0] & 0x10;
          angle=(rec[j].Data[2]<<8)|rec[j].Data[3];
//          std::cout << "angle: " <<angle << " deg" <<std::endl;
          power = rec[j].Data[1];
          if(power < 20){
//            std::cout << "power: " <<power << "%" <<std::endl;
            ROS_ERROR("Low battery, please charge");
            exit(-1);
          }
          else{
//            std::cout << "power: " <<power << "%" <<std::endl;
          }
        }
        if(forward == 1){
          speed = speed;
//          std::cout << "1" <<std::endl;

        }
        if(backward ==1){
          speed = speed;
//          std::cout << "0" <<std::endl;

        }

//        std::cout << "speed: " <<speed << " rpm/min" <<std::endl;

      }
    }
  }
  /*****************status reflection********************/

  /*****************thread waiting********************/
  void halt(void)
  {
    //control_thread_ -> join();
    odometer_thread_ -> join();
    //interpolation_thread_ ->join();
  }
  /*****************thread waiting********************/

  void start(void)
  {
    std::cout << "vehicle initialization ..." <<std::endl;
  }

};

int main(int argc, char** argv)
{
  ros::init(argc, argv, "laser");
  ros::Time::init();

  control_driver driver_node;
  ros::AsyncSpinner spinner(2);

  spinner.start();
  driver_node.start();

  driver_node.halt();
  ros::waitForShutdown();
  return 0;
}































































