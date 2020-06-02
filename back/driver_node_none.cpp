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

  std::thread* control_thread_;

public:
  control_driver()
  {
    control_thread_ = new std::thread(boost::bind(&control_driver::control_func,this));// control thread
  }

  ~control_driver(){}

  void control_func(){

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

      ros::Rate loop_rate(5);//0.2s
      while (ros::ok()) {

      walking_motor(1000, 0x02);
//      steering_motor(3000);
       vehicle_status_func();
//       up_down(2);
      ros::spinOnce();
      loop_rate.sleep();
    }
    VCI_CloseDevice(VCI_USBCAN1, 0);
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
        if(forward == 1){
          speed = speed;
          std::cout << "1" <<std::endl;

        }
        if(backward ==1){
          speed = speed;
          std::cout << "0" <<std::endl;

        }

//        std::cout << "speed: " <<speed << " rpm/min" <<std::endl;

      }
    }
  }
  /*****************status reflection********************/

  /*****************thread waiting********************/
  void halt(void)
  {
    control_thread_ -> join();
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

  driver_node.start();
  driver_node.vehicle_status_func();

  driver_node.halt();

  return 0;
}






























































