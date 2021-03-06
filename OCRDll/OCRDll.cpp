// OCRDll.cpp: 定义 DLL 应用程序的导出函数。
//

#include "OCRDll.h"
bool stop=false;
HANDLE g_hSemaphore = NULL;
HANDLE ocr_hSemaphore = NULL;
bool compareX(Rect a, Rect b)
{
	return a.x < b.x;
}
bool compareXBOX(bbox_t a, bbox_t b)
{
	return a.x < b.x;
}
void sortBox(vector<Rect> & oldRects, vector<vector<Rect>> & newRects)
{
	vector<Rect> row;
	vector<Rect> nextRows;
	int curMinY = 0, curMaxY = 0;
	for (int i = oldRects.size() - 1; i >= 0; --i)
	{
		if ((oldRects[i].y <= curMinY && oldRects[i].y + oldRects[i].height >= curMinY)
			|| (oldRects[i].y <= curMaxY && oldRects[i].y + oldRects[i].height >= curMaxY)
			|| (oldRects[i].y >= curMinY && oldRects[i].y + oldRects[i].height <= curMaxY))
		{
			newRects.back().push_back(oldRects[i]);
			if (curMinY > oldRects[i].y)
				curMinY = oldRects[i].y;
			if (curMaxY < oldRects[i].y + oldRects[i].height)
				curMaxY = oldRects[i].y + oldRects[i].height;

		}
		else
		{
			row = vector<Rect>();
			newRects.push_back(row);
			newRects.back().push_back(oldRects[i]);
			curMinY = oldRects[i].y;
			curMaxY = oldRects[i].y + oldRects[i].height;
		}
	}

	for (int i = 0; i < newRects.size(); ++i)
	{
		sort(newRects[i].begin(), newRects[i].end(), compareX);
	}

	for (int i = 0; i < newRects.size(); ++i)
	{
		for (int j = 0; j < newRects[i].size(); ++i)
		{
			if (j > 0 && newRects[i][j].x - newRects[i][j - 1].x > 300)
			{
				newRects[i][j].width = 1;
				newRects[i][j].height = 1;
			}
		}
	}
}
void bin_linear_scale(cv::Mat input_img, Mat& output_img, int width, int height)
{
	float len = 1.0 * std::max(input_img.cols, input_img.rows);
	float len_ = std::min(width, height);

	float ratio = len_ / len;

	int w_ = ratio * input_img.cols;
	int h_ = ratio * input_img.rows;

	resize(input_img, output_img, Size(w_, h_), 0, 0, INTER_AREA);
}

void getConnectedDomain(Mat src, vector<Rect>& boundingbox, Point offset = Point(0, 0))//boundingbox为最终结果，存放各个连通域的包围盒  
{
	
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;

	findContours(src, contours, hierarchy, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE);

	vector<Point> maxcontours;   //最大轮廓  
	double maxArea = 0;

	for (size_t i = 0; i < contours.size(); i++)
	{
		double area = contourArea(contours[i]);

		if (area > maxArea)
		{
			maxArea = area;
			maxcontours = contours[i];
		}
	}


	vector<Rect> tb;
	for (size_t i = 0; i < contours.size(); ++i)
	{
		Rect a = boundingRect(contours[i]);
		if ((1.0*a.height / a.width) > 1.2)
			continue;
		else if (a.height > 60 || a.width<20 || (a.height<12 && a.width / a.height<10) || a.height<5)
			continue;

		a = Rect(a.x + offset.x, a.y + offset.y, a.width, a.height);
		//rectangle(result1, r, Scalar(255));
		boundingbox.push_back(a);
		//rectangle(draw, Point(a.x, a.y), Point(a.x + a.width, a.y + a.height), Scalar(255, 0, 0));
	}

	
}

void USMSharp(Mat &src, Mat &dst, int nAmount = 200)
{
	double sigma = 3;
	int threshold = 1;
	float amount = nAmount / 100.0f;

	Mat imgBlurred;
	GaussianBlur(src, imgBlurred, Size(), sigma, sigma);

	Mat lowContrastMask = abs(src - imgBlurred)<threshold;
	dst = src * (1 + amount) + imgBlurred * (-amount);
	src.copyTo(dst, lowContrastMask);
}

void removeMark(Mat& input, Mat& output)
{
	Mat hsv;
	cvtColor(input, hsv, CV_BGR2HSV);
	for (int i = 0; i < hsv.rows; i++)
	{
		uchar* values = hsv.ptr<uchar>(i);
		for (int j = 0; j < hsv.cols; j++)
		{
			if (!(values[j * 3 + 1] >= 43 && values[j * 3 + 1] <= 255 && values[j * 3 + 2] >= 43 && values[j * 3 + 2] <= 255))
			{
				values[j * 3] = 0;
				values[j * 3 + 1] = 0;
				values[j * 3 + 2] = 0;
			}
			else if (!(values[j * 3] >= 0 && values[j * 3] <= 10 || values[j * 3] >= 156 && values[j * 3] <= 180))
			{
				values[j * 3] = 0;
				values[j * 3 + 1] = 0;
				values[j * 3 + 2] = 0;
			}
			else
			{
				values[j * 3] = 0;
				values[j * 3 + 1] = 0;
				values[j * 3 + 2] = 255;
			}
		}
	}
	cvtColor(hsv, hsv, CV_HSV2BGR);
	cvtColor(hsv, hsv, CV_BGR2GRAY);
	
	inpaint(input, hsv, output, 3, INPAINT_TELEA);

}

void removeColor(Mat& input, Mat& output)
{
	Mat temp;
	cvtColor(input, temp, CV_BGR2HSV);

	for (int i = 0; i < temp.rows; i++)
	{
		uchar* values = temp.ptr<uchar>(i);
		for (int j = 0; j < temp.cols; j++)
		{
			/*
			opencv 的H范围是0~180，红色的H范围大概是(0~8)∪(160,180)
			S是饱和度，一般是大于一个值,S过低就是灰色（参考值S>80)，
			V是亮度，过低就是黑色，过高就是白色(参考值220>V>50)。
			*/


			if (values[j * 3 + 1] >= 0 && values[j * 3 + 1] <= 30 && values[j * 3 + 2] >= 221 && values[j * 3 + 2] <= 255)
				if (values[j * 3] >= 0 && values[j * 3] <= 180)
				{
					values[j * 3] = 0;
					values[j * 3 + 1] = 0;
					values[j * 3 + 2] = 255;
				}

		}
	}
	cvtColor(temp, temp, CV_HSV2BGR);
	output = temp;
}

void fixTransparent(Mat & input, Mat& output)
{
	if (input.channels() == 4)
	{
		Mat nA = Mat(input.rows, input.cols, CV_8UC3);
		//cvtColor(input, nA, CV_BGRA2BGR, 1);
		for (int i = 0; i < input.rows; ++i)
		{
			uchar* r = input.ptr<uchar>(i);
			uchar* d = nA.ptr<uchar>(i);
			for (int j = 0; j < input.cols; ++j)
			{
				if (r[j * 4 + 3])//不透明
				{
					nA.at<Vec3b>(i, j)[0] = r[j * 4];
					nA.at<Vec3b>(i, j)[1] = r[j * 4 + 1];
					nA.at<Vec3b>(i, j)[2] = r[j * 4 + 2];
				}
				else
				{
					nA.at<Vec3b>(i, j)[0] = 255;
					nA.at<Vec3b>(i, j)[1] = 255;
					nA.at<Vec3b>(i, j)[2] = 255;
				}

			}
		}
		output = nA;
	}
	else
		output = input;
}

void usm(Mat& input, Mat& out, int del, int type)
{

	removeColor(input, input);
	removeMark(input, input);

	Mat dst, binary;

	cvtColor(input, dst, CV_BGR2GRAY, 1);        //把原图转换成它的灰度图

	double MaxValue, MinValue;
	cv::minMaxLoc(dst, &MinValue, &MaxValue);

	int count[256] = { 0 };
	int total = dst.rows*dst.cols;
	double h;

	double mul = 1.0;

	USMSharp(dst, dst, 200);

	int width = dst.cols;
	int height = dst.rows;
	int totalGray = 0, blackNum = 0;
	for (int i = 0; i < height; ++i)
	{
		uchar* r = dst.ptr<uchar>(i);
		for (int j = 0; j < width; ++j)
		{
			if ((j > 0 && r[j] + del < r[j - 1])
				&& (j + 1 < height && r[j] + del < r[j + 1]))
			{
				totalGray += r[j];
				++blackNum;
			}
		}

	}

	for (int i = 0; i < width; ++i)
	{
		for (int j = 0; j < height; j++)
		{
			if (j > 0 && dst.at<uchar>(j, i) + del < dst.at<uchar>(j - 1, i)
				&&
				j + 1 < height && dst.at<uchar>(j, i) + del < dst.at<uchar>(j + 1, i))
			{
				totalGray += dst.at<uchar>(j, i);
				++blackNum;
			}
		}
	}

	int th = totalGray / blackNum;

	threshold(dst, dst, th, 255, type);

	out = dst;

}

void ContoursRemoveNoise(Mat & img, Mat & output, double pArea)
{
	int i, j;
	int color = 1;
	int nHeight = img.rows;
	int nWidth = img.cols;

	for (i = 0; i < nHeight; ++i)
	{
		uchar * value = img.ptr<uchar>(i);
		for (j = 0; j < nWidth; ++j)
		{
			if (!value[j])
			{
				//FloodFill each point in connect area using different color  
				floodFill(img, cvPoint(j, i), cvScalar(color));
				color++;
			}
		}
	}
	int ColorCount[255] = { 0 };
	for (i = 0; i < nHeight; ++i)
	{
		uchar * value = img.ptr<uchar>(i);
		for (j = 0; j < nWidth; ++j)
		{
			//caculate the area of each area  
			if (value[j] != 255)
			{
				ColorCount[value[j]]++;
			}
		}
	}
	//get rid of noise point  
	for (i = 0; i < nHeight; ++i)
	{
		uchar * value = img.ptr<uchar>(i);
		for (j = 0; j < nWidth; ++j)
		{
			if (ColorCount[value[j]] <= pArea)
			{
				value[j] = 255;
			}
		}
	}
	for (i = 0; i < nHeight; ++i)
	{
		uchar * value = img.ptr<uchar>(i);
		for (j = 0; j < nWidth; ++j)
		{
			if (value[j] < 255)
			{
				value[j] = 0;
			}
		}
	}
}
//中值滤波
void removePepperNoise(Mat& mask, Mat& out)
{
	medianBlur(mask, out, 3);//5
							 
}

void cut(Mat& input, vector<Rect>& boundingbox)
{
	dilate(input, input, getStructuringElement(MORPH_RECT, Size(13, 1)));//13
	
	removePepperNoise(input, input);
	getConnectedDomain(input, boundingbox);
}

void boxResize(vector<Rect>& boundingbox, vector<Rect>& dst, Size oldS, Size newS)
{
	vector<Rect> nB;
	vector<Rect>::iterator it;
	int x, y, w, h;
	for (it = boundingbox.begin(); it != boundingbox.end(); ++it)
	{
		x = 0.5 + 1.0*it->x / oldS.width*newS.width;
		y = 0.5 + 1.0*it->y / oldS.height*newS.height;
		w = 0.5 + 1.0*it->width / oldS.width*newS.width;
		h = 0.5 + 1.0*it->height / oldS.height*newS.height;
		nB.push_back(Rect(x, y, w, h));
	}
	dst = nB;
}

void fixSmall(Mat& src, Mat& dst)
{

	if (src.cols < 1024)
	{
		int channel = src.channels();
		if (channel == 3)
		{
			int newR = 1024;
			if (src.cols > 40)
				newR = 1.0 * 40 / 64 * 2048;
			Mat tmp(src.rows + 6, newR, CV_8UC3);
			for (int i = 0; i < tmp.rows; ++i)
			{
				uchar* values = tmp.ptr<uchar>(i);
				uchar* sv = NULL;
				if (i - 3 >= 0 && i - 3 < src.rows)
					sv = src.ptr<uchar>(i - 3);
				for (int j = 0; j < tmp.cols; ++j)
				{
					if (sv != NULL && j >= 0 && j < src.cols &&j >= 3)
					{
						int t = j - 3;
						values[j * 3] = sv[t * 3];
						values[j * 3 + 1] = sv[t * 3 + 1];
						values[j * 3 + 2] = sv[t * 3 + 2];
					}
					else
					{
						values[j * 3] = 255;
						values[j * 3 + 1] = 255;
						values[j * 3 + 2] = 255;
					}
				}
			}
			dst = tmp;
		}
		else if (channel == 1)
		{
			int newR = 1024;
			if (src.cols > 40)
				newR = 1.0 * 40 / 64 * 2048;
			Mat tmp(src.rows + 10, newR, CV_8UC1);
			for (int i = 0; i < tmp.rows; ++i)
			{
				uchar* values = tmp.ptr<uchar>(i);
				uchar* sv = NULL;
				if (i - 3 >= 0 && i - 3 < src.rows)
					sv = src.ptr<uchar>(i - 3);
				for (int j = 0; j < tmp.cols; ++j)
				{
					if (sv != NULL && j >= 0 && j < src.cols && j >= 3)
					{
						values[j] = sv[j - 3];
					}
					else
					{
						values[j] = 255;
					}
				}
			}
			dst = tmp;
		}
	}
	else
	{
		dst = src;
	}

}

void horizonMat(Mat& srcImg, int& startIndex, int& endIndex)
{
	int width = srcImg.cols;
	int height = srcImg.rows;
	int* projectValArry = new int[height];//创建用于储存每列白色像素个数的数组  
	memset(projectValArry, 0, height * 4);//初始化数组

	for (int row = 0; row < height; ++row)
	{
		uchar* val = srcImg.ptr(row);
		for (int col = 0; col < width; ++col)
		{
			if (val[col] == 255)//如果是白底黑字  
			{
				projectValArry[row]++;
			}
		}

	}
	int th = 3;
	if (srcImg.rows < 40)
		th = 1;

	for (int i = 0; i < srcImg.rows; ++i)
	{
		if (projectValArry[i] > th)
		{
			startIndex = i;
			break;
		}
	}
	for (int i = srcImg.rows - 1; i >= 0; --i)
	{
		if (projectValArry[i] > th)
		{
			endIndex = i;
			break;
		}
	}
	delete[] projectValArry;
}
void fixWord(Mat& src, Mat& dst)
{
	if (src.channels() > 1)
		cvtColor(src, src, CV_BGR2GRAY);
	int start = -1, end = -1;
	horizonMat(src, start, end);
	if (start == end)
	{
		dst = src;
		return;
	}
	int width = end - start + 1;
	if (width>src.cols / 8)
		src = src(Rect(0, start, src.cols, end - start + 1));
	resize(src, src, Size(80, 80));
	Mat tmp(100, 100, CV_8UC1);

	for (int i = 0; i < tmp.rows; ++i)
	{
		uchar* values = tmp.ptr<uchar>(i);
		uchar* sv = NULL;
		if (i - 10 >= 0 && i - 10 < src.rows)
			sv = src.ptr<uchar>(i - 10);
		for (int j = 0; j < tmp.cols; ++j)
		{
			if (sv != NULL && j - 10 >= 0 && j - 10 < src.cols)
			{
				values[j] = sv[j - 10];
			}
			else
			{
				values[j] = 0;
			}
		}
	}
	dst = tmp;
}

void findWidth(vector<bbox_t>& boxes, int* widths)
{
	bbox_t chinese;
	chinese.prob = 0;
	chinese.w = 0;
	widths[0] = widths[1] = 0;

	for (int i = 0; i < boxes.size(); ++i)
	{
		if (boxes[i].obj_id == 0)
		{
			if (chinese.prob < boxes[i].prob && 1.0*boxes[i].w / boxes[i].h>0.7 && 1.0*boxes[i].w / boxes[i].h<1.2)
				chinese = boxes[i];
		}
	}
	if (chinese.w == 0)
	{
		for (int i = 0; i < boxes.size(); ++i)
		{
			if (boxes[i].obj_id == 0)
			{
				if (chinese.prob < boxes[i].prob)
					chinese = boxes[i];
			}
		}
	}

	bbox_t others;
	others.prob = 0;
	others.w = 0;
	int count = 0;
	for (int i = 0; i < boxes.size(); ++i)
	{
		if (boxes[i].obj_id != 0)
		{
			++count;
			if (others.prob < boxes[i].prob)
				others = boxes[i];
		}
	}


	widths[0] = chinese.w + 1;
	widths[1] = others.w + 1;
}

vector<bbox_t> verticalMat(Mat srcImg)//垂直投影
{

	Mat binImg = srcImg;
	Mat ele = getStructuringElement(MORPH_RECT, Size(3, 1));//3
															//cvtColor(binImg, binImg, CV_BGR2GRAY);
	cv::dilate(binImg, binImg, ele);//erode函数直接进行腐蚀操作
									
	int perPixelValue;//每个像素的值
	int width = srcImg.cols;
	int height = srcImg.rows;
	int* projectValArry = new int[width];//创建用于储存每列白色像素个数的数组  
	memset(projectValArry, 0, width * 4);//初始化数组
	for (int col = 0; col < width; col++)
	{
		for (int row = 0; row < height; row++)
		{
			perPixelValue = binImg.at<uchar>(row, col);
			if (perPixelValue == 0)//如果是白底黑字  
			{
				projectValArry[col]++;
			}
		}

	}
	/*
	Mat verticalProjectionMat(height, width, CV_8UC1);//垂直投影的画布  
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			perPixelValue = 255;  //背景设置为白色  
			verticalProjectionMat.at<uchar>(i, j) = perPixelValue;
		}
	}

	for (int i = 0; i < width; i++)//垂直投影直方图
	{
		for (int j = 0; j < projectValArry[i]; j++)
		{
			perPixelValue = 0;  //直方图设置为黑色    
			verticalProjectionMat.at<uchar>(height - 1 - j, i) = perPixelValue;
		}
	}*/
	
	vector<bbox_t> ProjectionBoxes;
	vector<Mat> roiList;//用于储存分割出来的每个字符  
	int startIndex = 0;//记录进入字符区的索引  
	int endIndex = 0;//记录进入空白区域的索引  
	bool inBlock = false;//是否遍历到了字符区内  
	for (int i = 0; i < srcImg.cols; i++)//cols=width  
	{
		if (!inBlock && projectValArry[i] != 0)//进入字符区  
		{
			inBlock = true;
			startIndex = i;
		}
		else if (projectValArry[i] == 0 && inBlock)//进入空白区  
		{
			endIndex = i;
			inBlock = false;
			
			bbox_t box;
			box.x = startIndex;
			box.y = 0;
			box.w = endIndex + 1 - startIndex;
			box.h = srcImg.rows;

			if (ProjectionBoxes.size()>0 && ProjectionBoxes.back().w < height / 4)
			{
				bbox_t small = ProjectionBoxes.back();
				if (small.x + small.w + 6 > box.x)
				{
					ProjectionBoxes.pop_back();
					box.w = box.w + small.w;
					box.x = small.x;
				}
			}
			ProjectionBoxes.push_back(box);
			//rectangle(srcImg, Rect(box.x, box.y, box.w, box.h), Scalar(0, 0, 0));
		}
	}
	
	delete[] projectValArry;
	return ProjectionBoxes;
}

void selectBoxes(vector<bbox_t>& yolo, vector<bbox_t>& projection, Mat& orgImg, Classifier* cl, vector<vector<Word>>& allWord, bool anchor = true)
{
	string result = "";
	int widths[2];
	findWidth(yolo, widths);
	
	int i = 0, j = 0;
	
	if (widths[0] == 0 && widths[1] == 0 && projection.size()>0)
	{
		Mat roiImg = orgImg(Rect(projection[0].x, projection[0].y, projection[0].w, projection[0].h));
		
		fixWord(roiImg, roiImg);
		
		vector<Word> words;
		cl->classify(roiImg, words);

	}
	if (widths[0] <10 && widths[1]<10)
		return ;
	if (projection.size() == 0)
		return;
	else if (projection[0].h < 20)
		return ;
	if (widths[1] < 4)
		widths[1] = projection[0].h / 3;
	if (widths[0] < 4)
		widths[0] = projection[0].h;
	bbox_t window;
	window.x = 0;
	window.w = widths[0];
	vector<bbox_t> boxes;
	int perX = -1;
	while (i < yolo.size() && j < projection.size())
	{
		if (window.x == 0)
		{
			window.y = projection[0].y;
			window.h = projection[0].h;
			if (yolo[i].x <= projection[j].x)
			{
				window.x = yolo[i].x;
			}
			else
			{
				window.x = projection[j].x;
			}
		}
		if (allWord.size() > 54)
			return;
		
		if (perX == window.x)
			window.x += window.w + 1;
		if ((int)projection[j].x + (int)projection[j].w - (int)window.x > (int)projection[0].h / 10)//2
		{
			if ((int)yolo[i].x - (int)window.x> (int)projection[0].h / 4)//10
			{
				bbox_t tmp;
				if (j<projection.size() - 1 && projection[j].x + projection[j].w + 7 < projection[j + 1].x)
				{
					if (projection[j].w < 7)
					{

						if (j<projection.size() - 1 && projection[j + 1].x > projection[j].x + projection[j].w + projection[0].h / 5)
						{
							perX = window.x;
							window.x = projection[j + 1].x;
						}
						++j;
						continue;
					}
					else if (abs((int)window.x - (int)projection[j].x) < 10)
					{
						perX = window.x;
						tmp = projection[j];
						window.x = window.x + projection[j].w;
					}
					else
					{
						perX = window.x;
						tmp = window;
						tmp.x = tmp.x>1 ? tmp.x - 2 : 0;
						tmp.w += 1;
						window.x = window.x + window.w;
					}
				}

				else
				{
					perX = window.x;
					tmp = window;
					tmp.x = tmp.x>1 ? tmp.x - 2 : 0;
					window.x = window.x + window.w;
				}

				boxes.push_back(tmp);
				Mat roiImg = orgImg(Rect(tmp.x, tmp.y, (tmp.w + tmp.x) <= projection.back().x + projection.back().w ? tmp.w : projection.back().x + projection.back().w - tmp.x, tmp.h));
				fixWord(roiImg, roiImg);
				vector<Word> words;
				cl->classify(roiImg, words);

				if (words[0].val < 0.4)
				{
					if (widths[0] != 0 || widths[1] != 0)
					{
						if (widths[0] != 0)
						{
							unsigned int W = tmp.w;
							tmp.w = widths[0];
							roiImg = orgImg(Rect(tmp.x, tmp.y, (tmp.w + tmp.x) <= projection.back().x + projection.back().w ? tmp.w : projection.back().x + projection.back().w - tmp.x, tmp.h));
							fixWord(roiImg, roiImg);
							

							vector<Word> tmpW;
							cl->classify(roiImg, tmpW);
							if (tmpW[0].val > words[0].val)
							{
								words = tmpW;
							}
							else
							{
								tmp.w = W;
							}
							
						}

						if (words[0].val < 0.4)
						{
							unsigned int W = tmp.w;
							if (widths[1] != 0)
								tmp.w = widths[1] + 1;
							else if (widths[0] != 0 && widths[0]>projection[0].h / 2)
							{
								tmp.w = widths[0] * 2.0 / 3;
							}
							else
								tmp.w = projection[0].h / 2;

							roiImg = orgImg(Rect(tmp.x, tmp.y, (tmp.w + tmp.x) <= projection.back().x + projection.back().w ? tmp.w : projection.back().x + projection.back().w - tmp.x, tmp.h));
							fixWord(roiImg, roiImg);
							

							vector<Word> tmpW;
							cl->classify(roiImg, tmpW);

							if (tmpW[0].val > words[0].val)
							{
								words = tmpW;
							}
							else
							{
								tmp.w = W;
							}
							


						}
						else
						{
							window.x = tmp.x + tmp.w + 1;
						}

						if (words[0].val < 0.4)
						{
							if (j<projection.size() - 1 && abs((int)window.x - (int)projection[j + 1].x) < (int)projection[0].h / 4)
							{
								tmp.x = projection[j + 1].x;
								roiImg = orgImg(Rect(tmp.x, tmp.y, (tmp.w + tmp.x) <= projection.back().x + projection.back().w ? tmp.w : projection.back().x + projection.back().w - tmp.x, tmp.h));
								fixWord(roiImg, roiImg);
								

								vector<Word> tmpW;
								cl->classify(roiImg, tmpW);

								if (tmpW[0].val > words[0].val)
								{
									words = tmpW;
								}

								
							}

						}
						window.x = tmp.x + tmp.w + 1;
					}

				}

				allWord.push_back(words);
				//result += words[0].word;
				
			}
			else if ((int)window.x - (int)yolo[i].x > (int)projection[0].h / 2 || (i<yolo.size() - 1 && 2 * (yolo[i - 1].x + yolo[i - 1].w)>2 * yolo[i].x + yolo[i].w))
			{
				++i;
			}
			else
			{

				boxes.push_back(yolo[i]);
				if (yolo[i].obj_id == 0)
				{
					window.w = widths[0];
				}
				else
				{
					window.w = widths[1];
				}



				if (yolo[i].obj_id != 0)
				{
					yolo[i].x += 1;
					yolo[i].w += 1;
				}
				else
				{
					yolo[i].x += 1;
				}
				if (yolo[i].x < projection[j].x + 5 && yolo[i].x>projection[j].x)
				{
					yolo[i].w = (int)yolo[i].w + yolo[i].x - projection[j].x - 1;
					yolo[i].x = projection[j].x>0 ? projection[j].x - 1 : 0;

				}
				/*else if (projection[j].x<yolo[i].x + yolo[i].w && projection[j].x>yolo[i].x)
				{
				yolo[i].w = yolo[i].w - projection[j].x + yolo[i].x-1;
				yolo[i].x = projection[j].x>0 ? projection[j].x - 1 : 0;
				}*/
				Mat roiImg = orgImg(Rect(yolo[i].x, yolo[i].y, (yolo[i].x + yolo[i].w) <= projection.back().x + projection.back().w ? yolo[i].w : projection.back().x + projection.back().w - yolo[i].x, (yolo[i].h + yolo[i].y)>projection[0].h ? projection[0].h - yolo[i].y : yolo[i].h));

				fixWord(roiImg, roiImg);
				
				vector<Word> words;
				cl->classify(roiImg, words);
			

				if (words[0].val < 0.4)
				{
					if (widths[0] != 0 || widths[1] != 0)
					{
						vector<Word> oldWords = words;
						bbox_t oldPosition = yolo[i];
						int oldWidth = window.w;
						if (widths[0] != 0)
						{
							unsigned int W = yolo[i].w;
							yolo[i].w = widths[0];
							roiImg = orgImg(Rect(yolo[i].x, yolo[i].y, (yolo[i].x + yolo[i].w) <= projection.back().x + projection.back().w ? yolo[i].w : projection.back().x + projection.back().w - yolo[i].x, (yolo[i].h + yolo[i].y)>projection[0].h ? projection[0].h - yolo[i].y : yolo[i].h));
							fixWord(roiImg, roiImg);
							

							vector<Word> tmpW;
							cl->classify(roiImg, tmpW);
							if (tmpW[0].val > words[0].val)
							{
								words = tmpW;
								window.w = widths[0];
							}
							else
							{
								yolo[i].w = W;
							}
							
						}

						if (words[0].val < 0.4)
						{
							unsigned int X = yolo[i].x;
							unsigned int W = yolo[i].w;
							if (widths[1] != 0)
								yolo[i].w = widths[1] + 1;
							else if (widths[0] != 0)
								yolo[i].w = widths[0] * 2.0 / 3;
							else
								yolo[i].w = projection[0].h / 3;
							yolo[i].x = yolo[i].x > 0 ? yolo[i].x - 1 : 0;
							roiImg = orgImg(Rect(yolo[i].x, yolo[i].y, (yolo[i].x + yolo[i].w) <= projection.back().x + projection.back().w ? yolo[i].w : projection.back().x + projection.back().w - yolo[i].x, (yolo[i].h + yolo[i].y) > projection[0].h ? projection[0].h - yolo[i].y : yolo[i].h));
							fixWord(roiImg, roiImg);
							

							vector<Word> tmpW;
							cl->classify(roiImg, tmpW);
							if (tmpW[0].val > words[0].val)
							{
								words = tmpW;
								window.w = widths[1];
							}
							else
							{
								yolo[i].x = X;
								yolo[i].w = W;
							}
							
						}

						if (words[0].val < 0.2)
						{
							words = oldWords;
							yolo[i] = oldPosition;
							window.w = oldWidth;
						}
						/*
						if (i < yolo.size() - 1)
						{
						if (yolo[i].x + yolo[i].w < (int)yolo[i + 1].x - 10)
						{
						bbox_t newB;
						newB.x = yolo[i].x + yolo[i].w;
						newB.y = yolo[i].y;
						newB.w = yolo[i + 1].x - newB.x - 1;
						newB.h = yolo[i].h;
						vector <bbox_t>::iterator iter = yolo.begin() + i + 1;
						yolo.insert(iter, 1, newB);
						}
						}
						*/
					}

				}
				perX = window.x;
				window.x = yolo[i].x + yolo[i].w + 1;
				allWord.push_back(words);
				//result += words[0].word;
				
				++i;
			}

		}
		else
		{

			++j;
			while (j < projection.size())
			{


				if (abs((int)window.x - (int)projection[j].x) < (int)projection[0].h / 4 || projection[j].x>window.x || projection[j].x + projection[j].w > window.x + (int)projection[0].h / 4)// 5 10
				{
					if (!((int)window.x - (int)projection[j].x>(int)projection[0].h / 4))//10 5
						window.x = projection[j].x;
					break;
				}

				++j;
			}
		}
	}

	while (i < yolo.size())
	{
		while (i < yolo.size())
		{
			if (abs((int)window.x - (int)yolo[i].x) < 5 || yolo[i].x>window.x)
			{
				window.x = yolo[i].x;
				break;
			}
			++i;
		}
		if (i >= yolo.size())
			break;
		boxes.push_back(yolo[i]);

		Mat roiImg = orgImg(Rect(yolo[i].x, yolo[i].y, yolo[i].w, (yolo[i].h + yolo[i].y)>projection[0].h ? projection[0].h - yolo[i].y : yolo[i].h));
		fixWord(roiImg, roiImg);
		
		vector<Word> words;
		cl->classify(roiImg, words);
		allWord.push_back(words);
		//result += words[0].word;
		

		++i;
	}

	while (j < projection.size())
	{
		if (allWord.size() > 54)
			return;
		if (!((int)projection[j].x + (int)projection[j].w - (int)window.x >(int)projection[0].h / 10))
		{
			while (j < projection.size())
			{
				if (abs((int)window.x - (int)projection[j].x) < 5 || projection[j].x > window.x)
				{
					window.x = projection[j].x;
					break;
				}
				++j;
			}
		}
		if (j >= projection.size())
			break;

		Mat roiImg = orgImg(Rect(window.x, window.y, (window.x + window.w)<orgImg.cols ? window.w : orgImg.cols - window.x, window.h));
		fixWord(roiImg, roiImg);
		
		vector<Word> words;
		cl->classify(roiImg, words);
		bbox_t tmp = window;
		window.x = window.x + window.w;

		if (words[0].val < 0.4)
		{
			if (widths[0] != 0 || widths[1] != 0)
			{
				if (widths[0] != 0)
				{
					unsigned int W = tmp.w;
					tmp.w = widths[0];
					roiImg = orgImg(Rect(tmp.x, tmp.y, (tmp.w + tmp.x) <= projection.back().x + projection.back().w ? tmp.w : projection.back().x + projection.back().w - tmp.x, tmp.h));
					fixWord(roiImg, roiImg);
					

					vector<Word> tmpW;
					cl->classify(roiImg, tmpW);
					if (tmpW[0].val > words[0].val)
					{
						words = tmpW;
					}
					else
					{
						tmp.w = W;
					}
					
				}

				if (words[0].val < 0.4)
				{
					unsigned int W = tmp.w;
					if (widths[1] != 0)
						tmp.w = widths[1] + 1;
					else if (widths[0] != 0 && widths[0]>projection[0].h / 2)
					{
						tmp.w = widths[0] * 2.0 / 3;
					}
					else
						tmp.w = projection[0].h / 2;
					roiImg = orgImg(Rect(tmp.x, tmp.y, (tmp.w + tmp.x) <= projection.back().x + projection.back().w ? tmp.w : projection.back().x + projection.back().w - tmp.x, tmp.h));
					fixWord(roiImg, roiImg);
					

					vector<Word> tmpW;
					cl->classify(roiImg, tmpW);

					if (tmpW[0].val > words[0].val)
					{
						words = tmpW;
					}
					else
					{
						tmp.w = W;
					}
				


				}
				else
				{
					window.x = tmp.x + tmp.w + 1;
				}
				if (words[0].val < 0.4)
				{
					if (j<projection.size() - 1 && abs((int)window.x - (int)projection[j + 1].x) < (int)projection[0].h / 4)
					{
						tmp.x = projection[j + 1].x;
						roiImg = orgImg(Rect(tmp.x, tmp.y, (tmp.w + tmp.x) <= projection.back().x + projection.back().w ? tmp.w : projection.back().x + projection.back().w - tmp.x, (tmp.h + tmp.y) <= projection[0].y + projection[0].h ? tmp.h : projection[0].y + projection[0].h - tmp.y));
						fixWord(roiImg, roiImg);

						

						vector<Word> tmpW;
						cl->classify(roiImg, tmpW);

						if (tmpW[0].val > words[0].val)
						{
							words = tmpW;
						}

						
					}

				}

				window.x = tmp.x + tmp.w + 1;
			}

		}

		allWord.push_back(words);
		//result += words[0].word;

		
	}
	//return result;
}

void convertYoloBox(vector<bbox_t>& vB, int w, int h)
{
	for (int i = 0; i < vB.size(); ++i)
	{
		int x = (int)vB[i].x - 3;
		int y = (int)vB[i].y - 3;

		vB[i].x = (x >= 0 ? x : 0);
		vB[i].y = (y >= 0 ? y : 0);
		vB[i].w = (vB[i].w + vB[i].x < w ? vB[i].w : w - vB[i].x);
		vB[i].h = (vB[i].h + vB[i].y < h ? vB[i].h : h - vB[i].y);

		if (i < vB.size() - 1 && vB[i + 1].x<(int)vB[i].x + (int)vB[i].w - 2)
		{
			//vB[i + 1].w = vB[i].x + vB[i].w - vB[i + 1].x-1;
			//vB[i + 1].x = vB[i].x + vB[i].w+1;
		}
	}
}


int indexName[]{ -1,-1 };
int indexNum[]{ -1,-1,-1 };
int indexNum2[]{ -1,-1,-1,-1,-1,-1,-1,-1 };

int abstractMessage(vector<bbox_t>& yolo, vector<bbox_t>& projection, Mat& orgImg, Classifier* cl, vector<vector<Word>>& nameWords, vector<vector<Word>>& numWords, int flag = 0)
{
	vector<vector<Word>> allWords;
	selectBoxes(yolo, projection, orgImg, cl, allWords);

	if (flag == 0)
	{
		memset(indexName, 0, sizeof(int) * 2);
		memset(indexNum, 0, sizeof(int) * 3);
		memset(indexNum2, 0, sizeof(int) * 8);
	}

	for (int i = 0; i < allWords.size(); ++i)
	{
		if (flag == 0 || flag == 4)
		{
			for (int j = 0; j < allWords[i].size(); ++j)
			{
				if (allWords[i][j].word == "名")
				{
					indexName[0] = i;
					flag = 4;
				}
				else if (allWords[i][j].word == "称")
				{
					if (flag == 4)
						indexName[1] = indexName[0] + 1;
					else
						indexName[1] = i;
				}
				else if (allWords[i][j].word == "注")
				{
					indexNum[0] = i;
				}
				else if (allWords[i][j].word == "册")
				{
					indexNum[1] = i;
				}
				else if (allWords[i][j].word == "号")
				{
					indexNum[2] = i;
				}
				else if (allWords[i][j].word == "统")
				{
					indexNum2[0] = i;
				}
				else if (allWords[i][j].word == "一")
				{
					indexNum2[1] = i;
				}
				else if (allWords[i][j].word == "社")
				{
					indexNum2[2] = i;
				}
				else if (allWords[i][j].word == "会")
				{
					indexNum2[3] = i;
				}
				else if (allWords[i][j].word == "信")
				{
					indexNum2[4] = i;
				}
				else if (allWords[i][j].word == "用")
				{
					indexNum2[5] = i;
				}
				else if (allWords[i][j].word == "代")
				{
					indexNum2[6] = i;
				}
				else if (allWords[i][j].word == "码")
				{
					indexNum2[7] = i;
				}
			}

			if (indexName[1] - indexName[0] > 0)
			{
				flag = 1;
			}
			else if (indexNum[1] - indexNum[0] > 0 && indexNum[2] - indexNum[1] > 0)
			{
				flag = 2;
			}
			else if (indexNum2[1] - indexNum2[0] > 0 && indexNum2[2] - indexNum2[1] > 0 && indexNum2[3] - indexNum2[2] > 0 && indexNum2[4] - indexNum2[3] > 0 && indexNum2[5] - indexNum2[4] > 0 && indexNum2[6] - indexNum2[5] > 0 && indexNum2[7] - indexNum2[6] > 0)
			{
				flag = 2;
			}
		}
		else if (flag == 1)
		{
			nameWords.push_back(allWords[i]);
		}
		else if (flag == 2)
		{
			numWords.push_back(allWords[i]);
		}
	}
	return flag;
}

bool isSameRow(int y, int height, int targetY, int targetH)
{
	return ((targetY <= y && targetY + targetH >= y)
		|| (targetY <= y + height && targetY + targetH >= y + height)
		|| (targetY >= y && targetY + targetH <= y + height));
}
void mohu(Mat& src, Mat& dst)
{
	int ksize1 = 5;
	int ksize2 = 5;
	double sigma1 = 1;
	double sigma2 = 1.5;
	cv::GaussianBlur(src, dst, cv::Size(ksize1, ksize2), sigma1, sigma2);
	//blur(src, dst, Size(ksize1, ksize2));
	//bilateralFilter(src, dst, 5, 5, 5);
	
}
Detector *detector;
Classifier *cl;
void init(const string& labelPath, const string& graphPath, const string& cfg, const string& weight)
{
	//string labelPath = "D:\\Code\\Python\\print_char\\\print_char\\labels.txt";
	//string graphPath = "D:\\Code\\Python\\PythonApplication2\\PythonApplication2\\checkpoint\\frozen_model.pb";
	//detector = new Detector("D:\\Code\\C++\\darknet-master\\build\\darknet\\x64\\yolov3-obj.cfg", "D:\\Code\\C++\\darknet-master\\build\\darknet\\x64\\backup\\yolov3-obj_7400.weights");

	detector = new Detector(cfg, weight);
	detector->nms = 0.45;

	cl = new Classifier(graphPath, labelPath);

	g_hSemaphore = CreateSemaphore(
		NULL          //信号量的安全特性
		, 3            //设置信号量的初始计数。可设置零到最大值之间的一个值
		, 3            //设置信号量的最大计数
		, NULL         //指定信号量对象的名称
	);
	ocr_hSemaphore = CreateSemaphore(
		NULL          //信号量的安全特性
		, 1            //设置信号量的初始计数。可设置零到最大值之间的一个值
		, 1            //设置信号量的最大计数
		, NULL         //指定信号量对象的名称
	);
}

string filter(vector<vector<Word>> words, bool flag)
{
	string str = "";
	if (!flag)
	{
		for (int i = 0; i < words.size(); ++i)
		{
			if (words[i][0].word == "(")
				words[i][0].word = words[i][1].word;
			else if (words[i][0].word == ")")
				words[i][0].word = words[i][1].word;

			if (words[i][0].word == "c")
			{
				str += "C";
			}
			else if (words[i][0].word == "g")
			{
				str += "9";
			}
			else if (words[i][0].word == "h")
			{
				str += "H";
			}
			else if (words[i][0].word == "j")
			{
				str += "J";
			}
			else if (words[i][0].word == "k")
			{
				str += "K";
			}
			else if (words[i][0].word == "l")
			{
				str += "1";
			}
			else if (words[i][0].word == "o")
			{
				if (words[i][1].word != "")
				{
					str += words[i][1].word;
				}
				else
					str += "0";
			}
			else if (words[i][0].word == "s")
			{
				str += "S";
			}
			else if (words[i][0].word == "u")
			{
				str += "U";
			}
			else if (words[i][0].word == "v")
			{
				str += "V";
			}
			else if (words[i][0].word == "w")
			{
				str += "W";
			}
			else if (words[i][0].word == "x")
			{
				str += "X";
			}
			else if (words[i][0].word == "y")
			{
				str += "Y";
			}
			else if (words[i][0].word == "z")
			{
				str += "Z";
			}
			else
			{
				str += words[i][0].word;
			}
		}
	}
	else
	{
		for (int i = 0; i < words.size(); ++i)
		{
			if (words[i][0].word.length() < 2)
			{
				if (words[i][0].word == "(" || words[i][1].word == "(" || words[i][2].word == "(")
				{
					str += "(";
				}
				else if (words[i][0].word == ")" || words[i][1].word == ")" || words[i][2].word == ")")
				{
					str += ")";
				}
				else if (words[i][0].word == "c" || words[i][0].word == "C" || words[i][0].word == "e" || words[i][0].word == "f" || words[i][0].word == "r" || words[i][0].word == "t")
				{
					str += "(";
				}
				else if (words[i][0].word == "j" || words[i][0].word == "J" || words[i][0].word == "D")
				{
					str += ")";
				}
				else
				{
					str += words[i][0].word;
				}
			}
			else
			{
				str += words[i][0].word;
			}
		}
	}
	return str;
}

void begin(const string imgPath, string& CompanyName, string& CompanyNum)
{
	WaitForSingleObject(g_hSemaphore, INFINITE);
	if (stop)
	{
		CompanyName = "";
		CompanyNum = "";
		ReleaseSemaphore(g_hSemaphore, 1, NULL);
		return;
	}
	vector<Rect> rects;
	vector<Rect> originalRects;
	Mat input, originalImg, classifyImg;

	input = imread(imgPath, CV_LOAD_IMAGE_UNCHANGED);
	
	Size originalSize = input.size();
	fixTransparent(input, input);
	input.copyTo(originalImg);
	
	

	//bin_linear_scale(input, input, 1200, 1000);

	usm(originalImg, originalImg, 7, 0);

	threshold(originalImg, classifyImg, 10, 255, 1);
	bin_linear_scale(classifyImg, input, 1200, 1000);

	threshold(input, input, 115, 255, 0);
	
	cut(input, rects);

	boxResize(rects, originalRects, input.size(), originalSize);

	vector<vector<Rect>> rowRects;
	sortBox(originalRects, rowRects);

	ReleaseSemaphore(g_hSemaphore, 1, NULL);       //信号量值+1


	WaitForSingleObject(ocr_hSemaphore, INFINITE);
	if (stop)
	{
		CompanyName = "";
		CompanyNum = "";
		ReleaseSemaphore(ocr_hSemaphore, 1, NULL);       //信号量值+1
		return;
	}
	vector<vector<Word>> name;
	vector<vector<Word>> num;
	int flag = 0, nameF = 0, numF = 0;
	int nameX, nameY = 0, nameH = 0, nameW, numX, numY = 0, numH = 0, numW;
	for (int r = 0; r < rowRects.size(); ++r)
	{
		for (int i = 0; i <rowRects[r].size(); ++i)
		{
			Mat target = originalImg(rowRects[r][i]);

			Mat proj;
			Mat select;
			target.copyTo(proj);
			target.copyTo(select);

			fixSmall(target, target);

			cvtColor(target, target, CV_GRAY2BGR);

			
			vector<bbox_t> object = detector->detect(target, 0.15);
			sort(object.begin(), object.end(), compareXBOX);
			convertYoloBox(object, proj.cols, proj.rows);

			vector<bbox_t> vB = verticalMat(proj);
			Mat secImg = classifyImg(rowRects[r][i]);
			
			//string result = selectBoxes(object, vB, secImg, select,&cl);
			if (nameF == 1)
			{
				if (isSameRow(nameY, nameH, rowRects[r][i].y, rowRects[r][i].height))
				{
					if (name.size()>4 && (abs(nameH - rowRects[r][i].height) > 10 || nameX + nameW - rowRects[r][i].x>nameH / 2))
					{
						nameF = -1;
						continue;
					}
					abstractMessage(object, vB, secImg, cl, name, num, 1);
					nameX = rowRects[r][i].x;
					nameY = rowRects[r][i].y;
					nameH = rowRects[r][i].height;
					nameW = rowRects[r][i].width;
				}
				else if (nameX < rowRects[r][i].x)
				{
					nameF = 0;
				}
			}
			else if (numF == 2)
			{
				if (isSameRow(numY, numH, rowRects[r][i].y, rowRects[r][i].height))
				{
					abstractMessage(object, vB, secImg, cl, name, num, 2);
					numF = -1;
				}
				else if (numX < rowRects[r][i].x)
				{
					numF = 0;
				}
			}
			else
			{
				if (flag == 4)
				{
					if (isSameRow(nameY, nameH, rowRects[r][i].y, rowRects[r][i].height))
					{
						flag = abstractMessage(object, vB, secImg, cl, name, num, 4);
					}
					else if (nameX < rowRects[r][i].x)
					{
						flag = abstractMessage(object, vB, secImg, cl, name, num, 0);
					}
				}
				else
					flag = abstractMessage(object, vB, secImg, cl, name, num, 0);
				if (flag == 1 && nameF != -1)
				{
					nameF = flag;
					nameX = rowRects[r][i].x;
					nameY = rowRects[r][i].y;
					nameH = rowRects[r][i].height;
					nameW = rowRects[r][i].width;

				}
				else if (flag == 2 && numF != -1)
				{
					numF = flag;
					numX = rowRects[r][i].x;
					numY = rowRects[r][i].y;
					numH = rowRects[r][i].height;
					numW = rowRects[r][i].width;
					if (num.size() > 7)
						numF = -1;
				}
				else if (flag == 4)
				{
					nameX = rowRects[r][i].x;
					nameY = rowRects[r][i].y;
					nameH = rowRects[r][i].height;
					nameW = rowRects[r][i].width;
				}
			}


		}
		if (name.size() > 4 && num.size() > 4)
			break;
	}
	CompanyName = filter(name, true);
	CompanyNum = filter(num, false);
	/*for (int i = 0; i < name.size(); ++i)
	{
		CompanyName+= name[i][0].word;
	}
	
	for (int i = 0; i < num.size(); ++i)
	{
		CompanyNum+= num[i][0].word;
	}
	*/
	ReleaseSemaphore(ocr_hSemaphore, 1, NULL);       //信号量值+1

}

void destroy()
{
	delete cl;
	delete detector;
}

void ocr(const string imgPath, string& name, string& num)
{
	begin(imgPath, name, num);
	trim(name);
	trim(num);
}

std::string& trim(std::string &s)
{
	if (s.empty())
	{
		return s;
	}

	s.erase(0, s.find_first_not_of(" "));
	s.erase(s.find_last_not_of(" ") + 1);
	return s;
}

void setIsStop(bool isStop)
{
	stop = isStop;
}