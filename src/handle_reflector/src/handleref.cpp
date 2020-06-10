#include "handleref.h"

using namespace std;

//这里是所有的常数定义
float RADIUS = 0.5;

float ANGLE_MIN;
float ANGLE_INCREMENT;

//这里是所有的全局变量
//1. 储存了全局地图的相关信息
float x_1 = 5; float y_1 = 5;
float x_2 = 5; float y_2 = -5;
float x_3 = -5; float y_3 = 5;
float x_4 = -5; float y_4 =-5;

//2. 用来储存发布信息
geometry_msgs::PointStamped robot_location;
geometry_msgs::PointStamped test_point;

//3. 用来申明将会在各个函数中交换信息的变量

//地图中所有反光柱的位置序列 
vector<float> xs;
vector<float> ys;

//可见反光柱的相关位置与距离信息
Eigen::Matrix<float, 3, 4> M_ref_sensor;
Eigen::Matrix<float, 3, 4> M_ref_real;
//Eigen::Matrix<float, 1, 4> M_dis_sensor;
vector<float> M_dis_sensor;
Eigen::Matrix<float, 1, 4> M_dis_real;

float d1, d2, d3, d4;

//什么是有效距离？
std::vector<float> valid_dis;


HandleRef::HandleRef()
{
	scan_sub = n.subscribe<sensor_msgs::LaserScan>("/scan",1,&HandleRef::scan_Callback,this);
	robot_pub = n.advertise<geometry_msgs::PointStamped>("robot_location", 1);
	test_point_pub = n.advertise<geometry_msgs::PointStamped>("test_point", 1);
}

struct point
{
	point() {}
	std::vector<float> x;
	std::vector<float> y;
	std::vector<int> ID;
};
point reflectors;

// 获得两点间的直线距离
float HandleRef::getDis(float x1, float y1, float x2, float y2)
{
	float Dis;
	Dis = sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));

	return Dis;

}

// 从获取的激光数据中，搜索出圆弧片段的起点与终点
bool HandleRef::findcircles(std::vector<float> num, int count, vector<Eigen::Matrix2d>& count_start_end)
{
	Eigen::Matrix2d start_end;

	bool sign = 0;

	cout<<"找到了以下的圆弧："<<endl;

	for(int i=1;i<count;i++){
		if(num.at(i)<100 && sign == 0 && num.at(i)>0.3){
			start_end(0,0) = i;
			cout<<"start="<<i/2<<endl;
			sign = 1;
		}

		if(num.at(i)>100 && sign == 1 && num.at(i)>0.3){
			start_end(1,0) = i;
			count_start_end.push_back(start_end);
			cout<<"end="<<i/2<<endl;
			sign = 0;
		}
	}

	cout<<"======"<<endl;
	return 0;
}


// 依照输入的离散点，拟合圆并给出圆心坐标
/**
 * Fit a circle in a set of points. You need a minimum of 1 point to fit a circle.
 *
 * @param points Is the set of points. //points是点的集合
 * @param midpoint Is the fitted midpoint of the circle. //minpoint是拟合出来的圆的中点
 * @param Returns true, if no error occur. An error occurs, if the points vector is empty. 返回真值，如果没有错误发生；如果points是空的，那么也会发生一个错误。
 */
bool HandleRef::solveLeastSquaresCircleKasa(const  std::vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d> > &points, Eigen::Vector2d &midpoint, double &radius)
{
	int length = points.size();
	double x1;
	double x2;
	double x3;
	Eigen::MatrixXd AFill(3, length);
	Eigen::MatrixXd a(length, 3);
	Eigen::VectorXd AFirst(length);
	Eigen::VectorXd ASec(length);
	Eigen::VectorXd AFirstSquared(length);
	Eigen::VectorXd ASecSquared(length);
	Eigen::VectorXd ASquaredRes(length);
	Eigen::VectorXd b(length);
	Eigen::VectorXd c(3);
	bool ok = true;

	if (length > 1)
	{
		for (int i = 0; i < length; i++)
		{
			AFill(0, i) = points[i](0);
			AFill(1, i) = points[i](1);
			AFill(2, i) = 1;
		}

		a = AFill.transpose();

		for (int i = 0; i < length; i++)
		{
			AFirst(i) = a(i, 0);
			ASec(i) = a(i, 1);
		}

		for (int i = 0; i < length; i++)
		{
			AFirstSquared(i) = AFirst(i) * AFirst(i);
			ASecSquared(i) = ASec(i) * ASec(i);
		}

		b = AFirstSquared + ASecSquared;

		c = a.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(b);

		x1 = c(0);
		midpoint(0) = x1 * 0.5;
		x2 = c(1);
		midpoint(1) = x2 * 0.5;
		x3 = c(2);
		radius = sqrt((x1 * x1 + x2 * x2) / 4 + x3);
	}
	else
	{
		ok = false;
	}

	return ok;
}

// 将输入的一点xy转换成odom坐标上的点，并push到reflector
void HandleRef::get_reflocation(float x, float y)
{
	tf::TransformListener tfListener;
	tf::StampedTransform scanTransform;

	tfListener.waitForTransform("odom", "laser_link", ros::Time(0), ros::Duration(1.0) );
	tfListener.lookupTransform("odom", "laser_link", ros::Time(0), scanTransform);

	float r_x = scanTransform.getOrigin().x();
	float r_y = scanTransform.getOrigin().y();
	float r_theta = tf::getYaw(scanTransform.getRotation());

	Eigen::Matrix<float,2,1> c;
	Eigen::Matrix<float,2,2> b;
	Eigen::Matrix<float,2,1> a;

	c << x,y;
	b << cos(r_theta), -sin(r_theta), sin(r_theta), cos(r_theta);
	a = b*c;

	cout<<"输出在odom下的reflector："<<a(0,0)+r_x<<" "<<a(1,0)+r_y<<endl;
	reflectors.x.push_back(a(0,0)+r_x);
	reflectors.y.push_back(a(1,0)+r_y);
}


void HandleRef::matching_ref( )
{
	float Dis;
	// 这里这个应该是也要修改的，需要有多少柱子装入多少
	M_ref_real << x_1, x_2, x_3, x_4, y_1, y_2, y_3, y_4, 1, 2, 3, 4;

	cout<<"size of reflectors_sensor:"<<reflectors.x.size()<<endl;
	for(int i=0; i<4; i++){
		M_ref_sensor(0,i)=reflectors.x.at(i);
		M_ref_sensor(1,i)=reflectors.y.at(i);
		M_ref_sensor(2,i)=i+1;
		cout<<"reflectors_sensor:"<<reflectors.x.at(i)<<reflectors.y.at(i)<<i+1<<endl;
	}


	std::cout<<"算出来的反光柱与真实反光柱位置的偏移："<<std::endl;
	for(int i=0;i<reflectors.x.size();i++)
	{
		for(int j=0;j<4;j++)
		{    
			Dis = HandleRef::getDis(M_ref_sensor(0,i),M_ref_sensor(1,i),M_ref_real(0,j),M_ref_real(1,j));

			std::cout<<Dis<<" ";
			if(Dis<2)
				M_dis_real(0,j) = M_dis_sensor.at(i);
		}
		std::cout<<std::endl;

	}

	cout<<"print mdissensor:"<<endl;
	for(int i=0;i<M_dis_sensor.size();i++){
	    cout<<M_dis_sensor.at(i);
	}
	cout<<"print mdisreal:"<<M_dis_real<<endl;
}


void HandleRef::scan_Callback(const sensor_msgs::LaserScan::ConstPtr& msg)
{
    //将一些全局变量清空，以便循环
	reflectors.x.clear(); reflectors.y.clear(); reflectors.ID.clear(); //清空reflectors
	xs.clear(); ys.clear();
	M_dis_sensor.clear();

	//这里以后要修改的，是把柱子的坐标放入
	xs.push_back(x_1);xs.push_back(x_2);xs.push_back(x_3);xs.push_back(x_4);
	ys.push_back(y_1);ys.push_back(y_2);ys.push_back(y_3);ys.push_back(y_4);


	M_dis_real << 0,0,0,0;
	cout<<"初始化M_dis_real:"<<M_dis_real<<endl;

	ANGLE_MIN = msg->angle_min;
	ANGLE_INCREMENT = msg->angle_increment;
	int count = msg->ranges.size();
	std::vector<float> ranges_vec = msg->ranges;
	vector<Eigen::Matrix2d> count_start_end;

	HandleRef::findcircles(ranges_vec, count, count_start_end);

	// 以上的代码没有问题了
	// 以下的代码开始搜索激光数据中的圆弧，然后输出一个Midpoints向量和m_dis_real的eigen向量（1xn）

	vector<Eigen::Vector2d> Midpoints;


	for(int i=0;i<count_start_end.size();i++){
		vector<Eigen::Vector2d> Points;

		int start_circle = count_start_end.at(i)(0,0);
		int end_circle = count_start_end.at(i)(1,0);
		for(int j=start_circle+1;j<end_circle;j++){

			float angle = j*ANGLE_INCREMENT;
			float point_x = -ranges_vec.at(j)*cos(angle);
			float point_y = -ranges_vec.at(j)*sin(angle);

			Eigen::Vector2d point;
			point << point_x, point_y;


			Points.push_back(point);
		}

		vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d> > samplePoints;
		for(int i=0; i<Points.size();i++){
			samplePoints.push_back(Points.at(i));
		}

		Eigen::Vector2d midpoint;
		double radius;
		bool ok;

		ok = solveLeastSquaresCircleKasa(samplePoints, midpoint, radius);

		Midpoints.push_back(midpoint);

		// 以下代码为了调试

		cout<<"输出本轮圆弧计算得到的midpoint："<<endl;
		float x = midpoint(0,0);
		float y = midpoint(1,0);

		// 装入mdis，及传感器扫描到的距离。
		M_dis_sensor.push_back(sqrt(x*x+y*y));


		cout<<"x:"<<x;
		cout<<" y:"<<y<<endl;

		if(i==0){
			test_point.point.x = x;
			test_point.point.y = y;
			test_point.point.z = 0;
			test_point.header.frame_id = "laser_link";
			test_point_pub.publish(test_point);
		}
		cout<<"------"<<endl;

		// cout<<"打印mdis"<<M_dis_sensor<<endl;

	}

	for(int i=0;i<Midpoints.size();i++){
		get_reflocation(Midpoints.at(i)(0.0),Midpoints.at(i)(1.0));
	}

	matching_ref();

	cout<<"debug point1!"<<endl;
	// 一下的部分是对机器人位置的最小二乘拟合，应该修改到先找有多少个维度，然后按照维度生成矩阵进行计算。
	// 先把距离构成d，然后对应的距离所对应的反光板id放入vector_id

	// 接着这里去除mdisreal中的0项
	vector<float> vector_d;
	vector<int> vector_id;
	cout<<"mdisreal_cols:"<<M_dis_real.cols()<<endl;
	for(int i=0;i<M_dis_real.cols();i++){
		if(M_dis_real(0,i)>0){
			vector_d.push_back(M_dis_real(0,i));
			vector_id.push_back(i);
		}
	}

	cout<<"debug point2!"<<endl;

	int number_reflectors = vector_d.size();
	cout<<"number_reflectors:"<<number_reflectors<<endl;
    if(number_reflectors<4)
        return;

	cout<<"debug point3!"<<endl;
	
	
	// 处理动态矩阵的问题

	Eigen::MatrixXd matrix_a(number_reflectors,2);
	Eigen::MatrixXd matrix_b(number_reflectors,1);

	//Eigen::Matrix<float,4,2> matrix_a;
	//Eigen::MatrixXd matrix_a(4,2);
	//Eigen::Matrix<float,4,1> matrix_b;
	//Eigen::MatrixXd matrix_b(4,1);


	//matrix_a << 2*(x_1-x_4), 2*(y_1-y_4), 2*(x_2-x_4), 2*(y_2-y_4), 2*(x_3-x_4), 2*(y_3-y_4);

	cout<<"打印一下vector_d:";
	for(int i=0;i<number_reflectors;i++){
		cout<<vector_d.at(i)<<" ";	
	}	
	cout<<endl;
	
	cout<<"打印一下vector_id:";
	for(int i=0;i<number_reflectors;i++){
		cout<<vector_id.at(i)<<" ";
	}
	cout<<endl;
//	vector<int> test;
//	test.push_back(1);
//	test.push_back(2);
//	cout<<"test:"<<test.at(0)<<" "<<test.back()<<endl;

	for(int i=0;i<vector_id.size();i++){
		matrix_a(i,0)=2*(xs.at(vector_id.at(i))-xs.at(vector_id.back()));
		matrix_a(i,1)=2*(ys.at(vector_id.at(i))-ys.at(vector_id.back()));
	}

	cout<<"debug point4!"<<matrix_a<<endl;

	//matrix_b << std::pow(x_1,2)-std::pow(x_4,2)+std::pow(y_1,2)-std::pow(y_4,2)+std::pow(d4,2)-std::pow(d1,2), std::pow(x_2,2)-std::pow(x_4,2)+std::pow(y_2,2)-std::pow(y_4,2)+std::pow(d4,2)-std::pow(d2,2), std::pow(x_3,2)-std::pow(x_4,2)+std::pow(y_3,2)-std::pow(y_4,2)+std::pow(d4,2)-std::pow(d3,2);

	for(int i=0;i<number_reflectors;i++){
		matrix_b(i,0) = std::pow(xs.at(vector_id.at(i)),2)-std::pow(xs.at(vector_id.back()),2)+std::pow(ys.at(vector_id.at(i)),2)-std::pow(ys.at(vector_id.back()),2)+std::pow(vector_d.at(vector_id.back()),2)-std::pow(vector_d.at(vector_id.at(i)),2);
	}
	//matrix_b << std::pow(x_1,2)-std::pow(x_4,2)+std::pow(y_1,2)-std::pow(y_4,2)+std::pow(d4,2)-std::pow(d1,2), std::pow(x_2,2)-std::pow(x_4,2)+std::pow(y_2,2)-std::pow(y_4,2)+std::pow(d4,2)-std::pow(d2,2), std::pow(x_3,2)-std::pow(x_4,2)+std::pow(y_3,2)-std::pow(y_4,2)+std::pow(d4,2)-std::pow(d3,2);
	cout<<"debug point4.1!"<<matrix_b<<endl;
	Eigen::MatrixXd matrix_ata;
	Eigen::MatrixXd location_r;

	cout<<"debug point5!"<<endl;


	matrix_ata = matrix_a.transpose()*matrix_a;

	location_r = matrix_ata.inverse()*matrix_a.transpose()*matrix_b;

	cout<<"debug point6!"<<endl;

	std::cout<<"输出位置： "<<location_r.transpose()<<std::endl;
	std::cout<<"======"<<std::endl;

	robot_location.point.x = location_r(0,0);
	robot_location.point.y = location_r(1,0);
	robot_location.point.z = 0;
	robot_location.header.frame_id = "odom";
	robot_pub.publish(robot_location);
	cout<<"debug point7!"<<endl;

}


int main(int argc, char** argv)
{
	ros::init(argc, argv, "handleref");
	HandleRef handle_laser; //实体化一个在头文件中的class，在class里面就包含了回调函数的声明

	ros::Rate loop_rate(1);
	while (ros::ok())
	{
		ros::spinOnce();
		loop_rate.sleep();
	}
	return 0;
}
