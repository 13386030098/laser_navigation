#include "ros/ros.h"
#include <sstream>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <ctime>
#include "controlcan.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include "unistd.h"

#include <thread>

class control_driver
{
private:
  short speed;
  short angle;
  short current;
  short direction;

  ros::NodeHandle nh_;

  std::thread* vehicle_status_thread_;
  std::thread* control_thread_;

public:
  control_driver()
  {
    vehicle_status_thread_ = new std::thread(boost::bind(&control_driver::vehicle_status_func,this));// read footswitch thread
    control_thread_ = new std::thread(boost::bind(&control_driver::control_func,this));// control thread
  }

  ~control_driver(){}

  void vehicle_status_func(){
    int j;
    int ind=0;
    int reclen=0;
    VCI_CAN_OBJ rec[3000];

    ros::Rate loop_rate(5);//0.002s
    while (ros::ok())
    {
      if((reclen=VCI_Receive(VCI_USBCAN1,0,ind,rec,3000,100))>0)
      {
        for(j=0;j<reclen;j++)
        {
          if(rec[j].ID==0x1A6)//0x26
          {
            speed=(rec[j].Data[0]<<8)|rec[j].Data[1];
            std::cout << speed <<std::endl;

          }
          if(rec[j].ID==0x2A6)
          {
            direction = rec[j].Data[0]&0x01;
            angle=(rec[j].Data[1]<<8)|rec[j].Data[2];
            std::cout << angle <<std::endl;
          }
          if(rec[j].ID==0x3A6)
          {

          }
        }
      }
      ros::spinOnce();
      loop_rate.sleep();
    }

  }

  void control_func(){
    ros::Rate loop_rate(5);//0.002s
    while (ros::ok()) {

      walking_motor(0, 0x02);
      steering_motor(4000);

      ros::spinOnce();
      loop_rate.sleep();
    }
  }

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

  void halt(void)
  {
    vehicle_status_thread_ -> join();
    control_thread_ -> join();
  }

  void start(void)
  {
    std::cout << "vehicle control start ..." <<std::endl;
  }

};

int main(int argc, char **argv)
{
  if(VCI_OpenDevice(VCI_USBCAN1,0,0)==1)
  {
    printf(">>open deivce success!\n");
  }
  else
  {
    printf(">>open deivce error!\n");
    exit(1);
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

	ros::init(argc, argv,"laser");
  control_driver driver_node;
  driver_node.start();
  driver_node.halt();

  VCI_CloseDevice(VCI_USBCAN1, 0);
  return 0;
}






























































