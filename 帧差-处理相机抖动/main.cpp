/*
落盘偏差检测
帧差法
2017_2_21 加入“鼠标选取ROI”程序段
2017_2_21 准备添加“再次颜色判断”程序段  程序段未完成 定位于401行
2017-2-22 找到更准确的轮廓判断，好像次轮廓的质心判断有问题
2017-2-22 发现问题所在，加入次轮廓是否为0的判断，分别输出两种情况的轮廓 ，好像还有些问题 ，次轮廓判断有问题
有些符合条件的轮廓没有找到
2017-2-25 采集了仿生的发射飞盘的视频，处理大的抖动情况，发现抖动很大，背景轮廓覆盖不完全，灰色识别困难，RGB通道变化大，目前的方法是局部的颜色识别加上帧差
2017-2-26 加入效果不太好的灰度判断，使用了vector点集，可随时加入元素，用作储存中心点 定位于546行
*/

#include "opencv2/opencv.hpp"
#include "stdio.h"
#include <iostream>
using namespace cv;
using namespace std;

# define G_NUM_BACKGROUND 5//
# define G_YUZHI 20
typedef struct tagSIZE
{
	int x;
	int y;
} ROI_SIZE;

ROI_SIZE ROI = { 300, 200 };//   
Rect Rect_ROI(100, 150, ROI.x, ROI.y);
Rect Rect_1;
RotatedRect Rect_2;
int biaoding_flag = 0;
uchar Multi_Contours_flag = 0;
void on_MouseHandle(int event, int x, int y, int flags, void* param);
int main()
{
	Mat frame, frame_2, frame_3, gray_image, roi_image, rimage, f_image, gray_image_p, gray_image_threshold;
	Mat background, background_1, background_2, background_3, background_result_1, background_result_2;
	Mat histogramImage;//用来显示投影
	vector<Mat>  roi_image_channels;
	vector<vector<Point>> contours;
	vector<Point> maxcontours;
	vector<Point> SecondContours;
	vector<Point>Central_Point;
	unsigned int maxnum = 0;
	int num_frame_1 = 1;
	double fps;
	double t;
	char str[50];
	Point Goal = { 0, 0 };
	Point SecondContours_centroid = { 0, 0 };
	Point Final_centroid;
	int key;
	int g_n1 = 3, k = 0;//g_n1 平移的像素点数，n*2
	//int g_ColHeight[500] = { 0 };// 统计高度的数组指针
	int *g_ColHeight = new int[500];//没有释放
	int g_ColHeightLong = 0;//统计投影长度
	int Colour_Change = 0;
	namedWindow("原图", 0);
	namedWindow("处理区域", 0);
	namedWindow("结果", 0);
	namedWindow("相减后", 0);
	namedWindow("background_result_2", 0);

	VideoCapture capture("G://学习2//2016-10-2-OPENCV//2017-1-15-张迎港资料//一些素材//12.3//WIN_20161203_00_52_00_Pro.mp4");//G:/学习2/2016-10-2-OPENCV/2017-1-15-张迎港资料/一些素材/12.3/WIN_20161203_00_45_14_Pro   WIN_20161203_00_52_00_Pro Video_2017-02-15_194658.wmv
	//VideoCapture capture("G:/学习2/2017-2-25-仿生发射视频/Video_2017-02-25_114413.wmv");//   
	//VideoCapture capture("G:/学习2/2017-1-17-免驱摄像头图像目录/2017-1-17-capture-/20170302181622.avi");
	//t = (double)getTickCount();
	//VideoCapture capture(1);
	setMouseCallback("原图", on_MouseHandle, (void*)&frame);

	while (1)
	{
		capture >> frame;
		key = waitKey(1000);
		switch (key)
		{
		case 'b': biaoding_flag = !biaoding_flag; break;
		}

		rectangle(frame, Rect_ROI, Scalar(0, 255, 255), 2, 5);
		imshow("原图", frame);
		if (biaoding_flag == 1)
		{
			cout << "ROI选取成功！";
			break;
		}
	}
	for (int i = 0; i < G_NUM_BACKGROUND; i++)
	{
		capture >> frame;
		if (frame.empty())
		{
			printf("\n无信号输入\n");
			break;
		}
		//
		roi_image = frame(Rect_ROI);//注意3个Rect一定要相等 Rect(200 , 300 , ROI.x, ROI.y) Rect_ROI
		if (background_result_2.empty())
		{

			background_result_2.create(roi_image.rows, roi_image.cols, CV_8UC1);
			background_result_2.operator = (0);

		}
		cvtColor(roi_image, gray_image, CV_BGR2GRAY);
		if (background_1.empty())
		{
			gray_image.copyTo(background_1);
		}
		if (i == 0) continue;//结束本次循环
		absdiff(background_1, gray_image, background_result_1);//		帧差法关键步骤
		gray_image.copyTo(background_1);
		imshow("背景相减后", background_result_1);



		//将background_result_1进行水平投影

		//数组清零 
		memset(g_ColHeight, 0, background_result_1.cols * 4);
		int value;
		for (int i = 0; i < background_result_1.rows; i++)
		{
			for (int j = 0; j < background_result_1.cols; j++)
			{
				value = background_result_1.at<uchar>(i, j);
				if (value > 50)
				{
					g_ColHeight[j]++;//统计每列的白色像素点   
				}
			}
		}
		//将histogramImage赋初值
		if (histogramImage.empty())
		{
			histogramImage.create(background_result_1.rows, background_result_1.cols, CV_8UC1);
		}
		histogramImage.operator = (0);
		for (int i = 0; i < histogramImage.cols; i++)
		{

			for (int j = 0; j < g_ColHeight[i]; j++)//
			{
				histogramImage.at<uchar>(j, i) = 255;//设置为白色
			}
		}
		imshow("水平投影图", histogramImage);
		g_ColHeightLong = 0;// 清零
		//统计g_ColHeight的长度
		for (int j = 0; j < background_result_1.cols; j++)
		{
			if (g_ColHeight[j]>8)
			{
				g_ColHeightLong++;
			}
		}
		cout << "投影长度" << g_ColHeightLong << endl;

		if (g_ColHeightLong > background_result_1.cols / 2)//
		{
			addWeighted(background_result_1, 1, background_result_2,
				1, 0, background_result_2);
		}
		else
		{
			cout << "jingru";
			g_n1 = (-g_n1);
			roi_image = frame(Rect(Rect_ROI.x + g_n1, Rect_ROI.y + g_n1, ROI.x, ROI.y));//Rect(100, 150, ROI.x, ROI.y)  Rect_ROI
			cvtColor(roi_image, gray_image, CV_BGR2GRAY);
			absdiff(background_1, gray_image, background_result_1);//		帧差法关键步骤
			imshow("背景相减后", background_result_1);
			addWeighted(background_result_1, 1, background_result_2,
				1, 0, background_result_2);
		}





		/*capture >> background_1;
		capture >> background_2;
		capture >> background_3;
		imshow("background_1",background_1);
		imshow("background_2", background_2);
		imshow("background_3", background_3);*/
	}
	threshold(background_result_2, background_result_2, 50, 255, THRESH_BINARY);//将差值进行二值化。
	imshow("background_result_2", background_result_2);

	//*************************************************************************
	for (;;)
	{
		t = (double)getTickCount();
		//frame, frame_2, frame_3
		capture >> frame;
		/*capture >> frame_2;
		capture >> frame_3;*/
		if (frame.empty())
		{
			printf("\n无信号输入\n");
			break;
		}
		//此处是静态的、固定的ROI,后续最好可以实现动态的、自动的ROI锁定 ROI.x, ROI.y 代表宽度和高度
		roi_image = frame(Rect_ROI);//(Rect(100, 150, ROI.x, ROI.y)) (Rect(100, 100, ROI.x, ROI.y)) Rect_ROI
		//将截取的ROI彩色通道分离 彩色空间为BGR 蓝 绿 红 
		split(roi_image, roi_image_channels);
		/*roi_image = frame_2(Rect(170, 150, ROI.x, ROI.y));
		roi_image = frame_3(Rect(170, 150, ROI.x, ROI.y));*/
		cvtColor(roi_image, gray_image, CV_BGR2GRAY);

		if (background.empty())
		{
			gray_image.copyTo(background);
		}
		//由于相机拍摄的噪声主要是高斯分布，所以采用高斯滤波。
		//GaussianBlur(gray_image, gray_image, Size(3, 3), 0, 0);

		//morphologyEx(background, background, MORPH_ERODE, getStructuringElement(MORPH_RECT, Size(5, 5)));//腐蚀
		//morphologyEx(background, background, MORPH_DILATE, getStructuringElement(MORPH_RECT, Size(5, 5)));//膨胀  

		//gray_image-background的灰度图）的绝对值
		absdiff(background, gray_image, rimage);//		帧差法关键步骤
		// 对得到的前景进行阈值选取，去掉伪前景


		threshold(rimage, rimage, 10, 255, THRESH_BINARY);//将差值进行二值化。
		imshow("帧差后", rimage);
		/*imshow("相减后1", rimage);*/
		//遍历各个像素，将由于抖动的背景轮廓去除，结合彩色信息还原飞盘像素点  一个关键步骤

		//t = (double)getTickCount();  roi_image.at<Vec3b>(i, j)[0]
		////****************************************************************************************************

		for (int i = 0; i < rimage.rows; i++)
		{
			uchar* rimage_data = rimage.ptr<uchar>(i);//此时rimage已经二值化
			uchar* background_result_2_data = background_result_2.ptr<uchar>(i);

			uchar* roi_image_data_red = roi_image_channels[2].ptr<uchar>(i);
			uchar* roi_image_data_green = roi_image_channels[1].ptr<uchar>(i);
			uchar* roi_image_data_blue = roi_image_channels[0].ptr<uchar>(i);

			for (int j = 0; j < rimage.cols; j++)
			{
				if (
					//(rimage.at<char>(i, j) == background_result_2.at<char>(i, j)) 
					(rimage_data[j] >= 225) && (background_result_2_data[j] >= 225)

					)


				{
					/*int red = roi_image.at<Vec3b>(i, j)[2];
					int green = roi_image.at<Vec3b>(i, j)[1];
					int blue = roi_image.at<Vec3b>(i, j)[0];*/
					/*if (
					red>(green * 2) && red > (blue * 2 && red>50)
					)

					{
					rimage_data[j] = 255;
					jj++;
					cout << red << endl;

					}*/
					//灰色信息判断
					//if ((roi_image_data_blue[j]>88 && roi_image_data_blue[j]<110) && (roi_image_data_green[j]>85 && roi_image_data_green[j]<110) && (roi_image_data_red[j]>50 && roi_image_data_red[j]<92) )
					//{
					//	rimage_data[j] = 255;
					//	Colour_Change++;
					//	//
					//}
					//红盘的颜色判断
					if (roi_image_data_red[j]>(roi_image_data_green[j] * 2) && roi_image_data_red[j]>(roi_image_data_blue[j] * 2) && roi_image_data_red[j]>40)
					{


						rimage_data[j] = 255;
						//jj++;
						//cout << (int)roi_image_data_red[j] << endl;

					}

					else
						rimage_data[j] = 0;
				}
				if (

					(rimage_data[j] - background_result_2_data[j])>G_YUZHI//大于就行了
					)

					rimage_data[j] = 255;

			}

		}


		//****************************************************************************************************

		imshow("相减后", rimage);

		/*imshow("红色通道", roi_image_channels[2]);
		imshow("绿色通道", roi_image_channels[1]);
		imshow("蓝色通道", roi_image_channels[0]);*/



		//注释掉即为背景侦察法。不注释就是帧差法。
		gray_image.copyTo(background);

		// 看膨胀后的效果
		//morphologyEx(rimage, gray_image_p, MORPH_DILATE, getStructuringElement(MORPH_RECT, Size(3, 3)));//膨胀  
		//imshow("膨胀后", gray_image_p);


		//开运算
		//morphologyEx(rimage, rimage, MORPH_ERODE, getStructuringElement(MORPH_RECT, Size(3, 3)));//腐蚀
		//morphologyEx(rimage, rimage, MORPH_DILATE, getStructuringElement(MORPH_RECT, Size(5, 5)));//膨胀  
		morphologyEx(rimage, rimage, MORPH_OPEN, getStructuringElement(MORPH_RECT, Size(5, 5)));//MORPH_DILATE MORPH_ERODE 5
		//morphologyEx(rimage, rimage, MORPH_CLOSE, getStructuringElement(MORPH_RECT, Size(5, 5)));//MORPH_DILATE MORPH_ERODE
		//morphologyEx(rimage, rimage, MORPH_DILATE, getStructuringElement(MORPH_RECT, Size(5, 5)));//膨胀  

		rimage.copyTo(f_image);

		findContours(f_image, contours, CV_RETR_EXTERNAL, CHAIN_APPROX_NONE, Point(0, 0));

		cout << "第" << num_frame_1 << "张图" << "找到" << contours.size() << "个轮廓" << endl;
		//筛选出最大的轮廓addWeighted

		for (int i = 0; i < contours.size(); i++)
		{
			if (contours[i].size()> maxcontours.size())
			{
				maxnum = i;
				maxcontours = contours[i];
			}
		}
		//将轮廓按大小排序SecondContours
		if (contours.size() > 1)
		{
			//排序
			for (uchar i = 0; i < contours.size(); i++)
			{
				vector<Point> Temp = contours[i];
				for (uchar j = (i + 1); j < contours.size(); j++)
				{
					if (contours[i].size() < contours[j].size())
					{
						contours[i] = contours[j];
						contours[j] = Temp;
					}
				}

			}
			maxcontours = contours[0];
			SecondContours = contours[1];
			//cout << "最大轮廓" << contours[0].size() << "  " << "次大轮廓" << contours[1].size() << endl;
			/*for (uchar i = 0; i < contours.size(); i++)
			{
			cout << "轮廓" << (int short)i << "大小为：" << contours[i].size() << endl;
			}*/

		}

		//if (contours.size()>1)
		//{
		//	//申请一段内存int *g_ColHeight = new int[500];
		//	int * contour_Length = new  int[contours.size()];
		//	//数组初始化
		//	memset(contour_Length, 0, contours.size() * 4);
		//	//将各轮廓大小复制到数组中
		//	for (uchar i = 0; i < contours.size(); i++)
		//	{
		//		contour_Length[i] = contours[i].size();
		//		/*cout << "轮廓的大小~~~~";
		//		cout << contours[i].size();*/

		//	}
		//	//冒泡法排序
		//	for (uchar i = 0; i < contours.size(); i++)
		//	{
		//		int Temp = contour_Length[i];
		//		for (uchar j = (i + 1); j < contours.size(); j++)
		//		{
		//			if (Temp < contour_Length[j])
		//			{
		//				contour_Length[i] = contour_Length[j];
		//				contour_Length[j] = Temp;
		//			}

		//		}
		//	}
		//	cout << "最大轮廓" << contour_Length[0] << "  " << "次大轮廓" << contour_Length[1] << endl;
		//	//释放contour_Length
		//	delete[]contour_Length;
		//}

		//用冒泡法进行排序
		/*for (int i = 0; i < maxcontours.size(); i++)
		{
		for (int j = i; j < maxcontours.size(); j++)
		{
		if (maxcontours[j].x>maxcontours[i].x)
		{
		int temp_value_x = maxcontours[i].x;
		maxcontours[j].x = maxcontours[i].x;
		maxcontours[i].x = temp_value_x;
		}
		if (maxcontours[j].y>maxcontours[i].y)
		{
		int temp_value_y = maxcontours[i].y;
		maxcontours[j].y = maxcontours[i].y;
		maxcontours[i].y = temp_value_y;
		}
		}
		}*/
		/*for (int ii = 0; ii < maxcontours.size(); ii++)
		{
		for (int jj = ii; jj < maxcontours.size(); jj++)
		{
		if (maxcontours[jj].x>maxcontours[ii].x)
		{
		int temp_value_x = maxcontours[ii].x;
		maxcontours[jj].x = maxcontours[ii].x;
		maxcontours[ii].x = temp_value_x;
		}
		if (maxcontours[jj].y>maxcontours[ii].y)
		{
		int temp_value_y = maxcontours[ii].y;
		maxcontours[jj].y = maxcontours[ii].y;
		maxcontours[ii].y = temp_value_y;
		}
		}
		}*/


		if (maxcontours.size() >60)
		{
			Rect_1 = boundingRect(maxcontours);
			//Rect_2 = fitEllipse(maxcontours);
			rectangle(roi_image, Rect_1, Scalar(0, 0, 255), 2, 8);
			//ellipse(roi_image, Rect_2, Scalar(0, 0, 255), 1, 8);
			//算出矩形的中心区域
			int x, y;
			x = Rect_1.x + (Rect_1.width / 2);
			y = Rect_1.y + (Rect_1.height / 2);
			cout << "中心坐标为:" << x << "  " << y << endl;
			//画出要进行颜色判断的区域
			//Rect Rect_2((Rect_1.x - 50), (Rect_1.y - 50)), Rect_1.width * 2, Rect_1.height * 2);
			//rectangle(roi_image, Rect_2, Scalar(0, 0, 255), 2, 8);
			//定义变量
			int Roi_Rows_j_start, Roi_Rows_j_end, Roi_Cols_i_start, Roi_Cols_i_end;
			//赋初值
			Roi_Rows_j_start = y-70;
			Roi_Rows_j_end =   y+70;
			Roi_Cols_i_start = x-70;
			Roi_Cols_i_end =   x+70;
			
			//判断是否超界
			if (Roi_Cols_i_start < 0)
			{
				Roi_Cols_i_start = 0;
			}
			if (Roi_Cols_i_end >(rimage.cols - 1))
			{
				Roi_Cols_i_end = (rimage.cols - 1);
			}
			if (Roi_Rows_j_start < 0)
			{
				Roi_Rows_j_start = 0;
			}
			if (Roi_Rows_j_end >(rimage.rows - 1))
			{
				Roi_Rows_j_end = (rimage.rows - 1);
			}
			Rect Rect_2(Roi_Cols_i_start, Roi_Rows_j_start, Roi_Cols_i_end - Roi_Cols_i_start, Roi_Rows_j_end - Roi_Rows_j_start);
			rectangle(roi_image, Rect_2, Scalar(0, 0, 255), 2, 8);
			for (int i = Roi_Rows_j_start; i <Roi_Rows_j_end; i++)
			{
				uchar* rimage_data_2 = rimage.ptr<uchar>(i);//此时rimage已经二值化
				//三个颜色通道 取指针
				uchar* roi_image_data_red_2 = roi_image_channels[2].ptr<uchar>(i);
				uchar* roi_image_data_green_2 = roi_image_channels[1].ptr<uchar>(i);
				uchar* roi_image_data_blue_2 = roi_image_channels[0].ptr<uchar>(i);
				for (int j = Roi_Cols_i_start; j < Roi_Cols_i_end; j++)
				{
					//进行颜色判断 红盘的判断
					if (roi_image_data_red_2[j]>(roi_image_data_green_2[j] * 2) && roi_image_data_red_2[j]>(roi_image_data_blue_2[j] * 2) && roi_image_data_red_2[j]>40)
					{
						rimage_data_2[j] = 255;

					}
					//灰盘的颜色判断
					//if ((roi_image_data_blue_2[j]>88 && roi_image_data_blue_2[j]<110) && (roi_image_data_green_2[j]>85 && roi_image_data_green_2[j]<110) && (roi_image_data_red_2[j]>50 && roi_image_data_red_2[j]<92))
					//{
					//	rimage_data_2[j] = 255;
					//	
					//	//
					//}

				}
			}
		}


		//查找质心
		for (int i = 0; i < maxcontours.size(); i++)
		{
			//cout << maxcontours[i].x<< endl;
			Goal.x += maxcontours[i].x;
			Goal.y += maxcontours[i].y;

		}
		//查找质心
		for (int i = 0; i<SecondContours.size(); i++)
		{
			SecondContours_centroid.x += SecondContours[i].x;
			SecondContours_centroid.y += SecondContours[i].y;
		}

		if (maxcontours.size() > 50)
		{
			cout << maxcontours.size() << endl;
			Goal.x /= maxcontours.size();
			Goal.y /= maxcontours.size();
			Central_Point.push_back(Point(Goal.x, Goal.y));
			cout << "目标点的坐标为" << "X:" << Goal.x << "Y:" << Goal.y << endl;
			//circle(rimage, Goal, 10, Scalar(255, 255, 255), 5, 8);
			//if (SecondContours.size() > 30)
			//{
			//	SecondContours_centroid.x /= SecondContours.size();
			//	SecondContours_centroid.y /= SecondContours.size();
			//	cout << "次轮廓的质心坐标为" << "X:" << SecondContours_centroid.x << "Y:" << SecondContours_centroid.y << endl;
			//	circle(rimage, SecondContours_centroid, 10, Scalar(255, 255, 255), 5, 8);
			//}
			//if (SecondContours.size() > 30)
			//{
			//	Final_centroid.x = (Goal.x + SecondContours_centroid.x) / 2;//
			//	Final_centroid.y = (Goal.y + SecondContours_centroid.y) / 2;//
			//	circle(roi_image, Final_centroid, 40, Scalar(0, 0, 255), 5, 8);
			//}
			/*if (SecondContours.size() < 30)
			{*/
			Final_centroid.x = Goal.x;
			Final_centroid.y = Goal.y;
			circle(roi_image, Final_centroid, 40, Scalar(0, 0, 255), 5, 8);
			/*}*/

		}
		else
		{
			cout << "未发现目标点0" << endl;
		}




		num_frame_1++;

		//变量复位。
		Goal = { 0, 0 };
		SecondContours_centroid = { 0, 0 };
		maxnum = 0;
		maxcontours.clear();
		SecondContours.clear();

		//显示帧率
		t = ((double)getTickCount() - t) / getTickFrequency();
		fps = 1.0 / t;
		sprintf_s(str, "FPS:%5f", fps);
		putText(frame, str, Point(0, 30), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255));

		imshow("原图", frame);
		imshow("处理区域", roi_image);
		imshow("结果", rimage);

		key = waitKey(30);
		switch (key)
		{
			///*case 'c':	sprintf_s(filename, "D://capture//min//%d.bmp", num_frame);
			//	imwrite(filename, roi_image);*/
			//	/*printf("第%d采集完成\n", num_frame);
			//	break;*/
			////case ' ':Start_flag = !Start_flag; Jizhong_flag = 0; break;
			//case 'b': biaoding_flag = !biaoding_flag; break;
			////case 'g': biaodingqiu_flag = !biaodingqiu_flag;

			//	/*break;*/
		case 'c':
		{
					for (int i = 0; i < Central_Point.size(); i++)
					{
						cout << Central_Point[i] << endl;
					}

		}
			break;
		}

	}





}

void on_MouseHandle(int event, int x, int y, int flags, void* param)
{
	switch (event)
	{
		//左键按下消息
	case EVENT_LBUTTONDOWN:
	{
							  if (biaoding_flag == 0)
							  {
								  Rect_ROI.x = x;//记录起始点
								  Rect_ROI.y = y;//记录起始点
								  //biaoding_flag = 0;
							  }
							  //if (biaodingqiu_flag == 1)
							  //{
							  // Panduan_goal.x = x;//记录起始点
							  // Panduan_goal.y = y;//记录起始点
							  // biaodingqiu_flag = 0;
							  //}
	}
		break;
	}
}

