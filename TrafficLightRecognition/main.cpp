#include <opencv\cv.h>
#include <opencv\highgui.h>

using namespace cv;

#define THRESHOLD_LIGHT 100
#define THRESHOLD_BOX 70
#define TOP_HAT_SIZE 3
#define DISTANCE_WITH_LIGHT 60
#define DISTANCE_REJECT_CANDIDATE_WITH_COLOR 90


int morph_size = TOP_HAT_SIZE;
Mat element_tophat = getStructuringElement(MORPH_RECT, Size(2 * morph_size + 1, 2 * morph_size + 1), Point(morph_size, morph_size));

int labelWidth;
int labelHeight;
int objectN = 0;
int blackObjectN = 0;

struct Object
{
public:
	int count;
	int centerX;
	int centerY;
	int width;
	int height;
	bool checking;
	bool isDeleted;
	Object() : count(0), centerX(0), centerY(0), width(0), height(0), checking(false), isDeleted(false) {};
};

//Labeling Direction
const int dx[] = { +1, 0, -1, 0 };
const int dy[] = { 0, +1, 0, -1 };



/* 색깔끼리의 거리(유사도)를 체크하는 함수(유클리드 Norm사용) */
double calcDistance(int b1, int g1, int r1, int b2, int g2, int r2)
{
	return sqrt((b1 - b2)*(b1 - b2) + (g1 - g2)*(g1 - g2) + (r1 - r2)*(r1 - r2));
}

/* recursive로 구현된 Labeing함수 */
void labeling(int x, int y,int current_label, Mat& sourceImage, int label[]) {
	if (x < 0 || x > sourceImage.cols - 1)
	{
		return;
	}// out of bounds
	if (y < 0 || y > sourceImage.rows - 1)
	{
		return;
	}// out of bounds
	if (label[y*sourceImage.cols + x] > 0 || sourceImage.at<uchar>(y, x) == 0)
	{
		return;
	}// already labeled or not marked with 1 in m

	// mark the current cell
	label[y*sourceImage.cols + x] = current_label;

	// recursively mark the neighbors
	for (int direction = 0; direction < 4; ++direction)
	{
		labeling(x + dx[direction], y + dy[direction], current_label, sourceImage,label);
	}
}

/* Object를 Reject하기위해 넓이와 높이 계산하는 함수 */
void checkArea(Object object[], Mat& src, Mat& original, int label[])
{
	for (int i = labelWidth / 10-1; i < labelWidth / 10 * 9+1; i++)
	{
		for (int j = 0; j < labelHeight / 3 * 2+1; j++)
		{
			if (label[j*labelWidth + i] > 0)
			{
				object[label[j*labelWidth + i]-1].count++;
				object[label[j*labelWidth + i]-1].centerX += i;
				object[label[j*labelWidth + i]-1].centerY += j;
				if (object[label[j*labelWidth + i]-1].checking)
					continue;
				else
				{
					object[label[j*labelWidth + i]-1].width++;
					object[label[j*labelWidth + i]-1].checking = true;
				}
			}
		}
		
		for (int k = 0; k < objectN; k++)
			if(object[k].checking) object[k].checking = false;
	}

	for (int i = 0; i < labelHeight / 3 * 2; i++)
	{
		for (int j = labelWidth / 10 - 1; j < labelWidth / 10 * 9 + 1; j++)
		{
			if (label[i*labelWidth + j] > 0)
			{
				if (object[label[i*labelWidth + j]-1].checking)
					continue;
				else
				{
					object[label[i*labelWidth + j]-1].height++;
					object[label[i*labelWidth + j]-1].checking = true;
				}
			}
		}

		for (int k = 0; k < objectN; k++)
			if (object[k].checking) object[k].checking = false;
	}

	for (int k = 0; k < objectN; k++)
	{
		object[k].centerX /= object[k].count;
		object[k].centerY /= object[k].count;
		
		if (object[k].count > 2)
		{
			int maxLenght = max(object[k].height, object[k].width);
			if (!(maxLenght * maxLenght >= object[k].count && 0.5890485 * maxLenght * maxLenght <= object[k].count))
			{
				object[k].isDeleted = true;
			}
		}
	}
}

/* Lebeing 시작하는 함수 */
int find_components(Mat& sourceImage, int label[]) {
	int component = 0;

	for (int i = 0; i < sourceImage.cols; ++i)
	{
		for (int j = 0; j < sourceImage.rows; ++j)
		{
			if (label[j*sourceImage.cols + i] == 0 && sourceImage.at<uchar>(j, i) > 0)
			{
				labeling(i, j, ++component, sourceImage, label);
			}
		}
	}

	return component;
}

/* 화면 바깥쪽의 잡티를 제거하고 신호등이 될수있는 색을 밝게 해주는 함수 */
void makeWhite(Mat& dst, Mat& src,Mat& tophatImage)
{
	for (int i = 0; i < dst.rows; i++)
	{
		for (int j = 0; j < dst.cols; j++)
		{
			if (i>dst.rows / 3 * 2)
			{
				dst.at<uchar>(i, j) = 0;
				continue;
			}

			if (j<dst.cols / 10 || j>dst.cols / 10 * 9)
			{
				dst.at<uchar>(i, j) = 0;
				continue;
			}

			int blue = src.at<Vec3b>(i, j)[0];
			int green = src.at<Vec3b>(i, j)[1];
			int red = src.at<Vec3b>(i, j)[2];

			double distanceYello = calcDistance(blue, green, red, 100, 220, 255);
			double distanceGreen = calcDistance(blue, green, red, 143, 250, 77);
			double distanceRed = calcDistance(blue, green, red, 250, 190, 76);
			
			if (tophatImage.at<uchar>(i, j) > 30)
			{
				if (distanceYello < DISTANCE_WITH_LIGHT || distanceGreen < DISTANCE_WITH_LIGHT || distanceRed < DISTANCE_WITH_LIGHT)
				{
					dst.at<uchar>(i, j) = 255;
				}
				else if (distanceYello < DISTANCE_WITH_LIGHT + 30 || distanceGreen < DISTANCE_WITH_LIGHT + 30 || distanceRed < DISTANCE_WITH_LIGHT + 30)
				{
					dst.at<uchar>(i, j) += 80;
				}
				else if (distanceYello < DISTANCE_WITH_LIGHT + 60 || distanceGreen < DISTANCE_WITH_LIGHT + 60 || distanceRed < DISTANCE_WITH_LIGHT + 60)
				{
					dst.at<uchar>(i, j) += 50;
				}
			}

		}
	}
}

/* 후보들에서 색깔을 읽어서 빨,주,노와 거리가 멀면 거르는 함수 */
void removeDifferColorCandidate(Mat& image, Object object[])
{
	for (int k = 0; k < objectN; k++)
	{
		int blue = image.at<Vec3b>(object[k].centerY, object[k].centerX)[0];
		int green = image.at<Vec3b>(object[k].centerY, object[k].centerX)[1];
		int red = image.at<Vec3b>(object[k].centerY, object[k].centerX)[2];

		double distanceYello = calcDistance(blue, green, red, 100, 220, 255);
		double distanceGreen = calcDistance(blue, green, red, 143, 250, 77);
		double distanceRed = calcDistance(blue, green, red, 250, 190, 76);

		if (distanceYello > DISTANCE_REJECT_CANDIDATE_WITH_COLOR && distanceGreen > DISTANCE_REJECT_CANDIDATE_WITH_COLOR && distanceRed > DISTANCE_REJECT_CANDIDATE_WITH_COLOR)
		{
			object[k].isDeleted = true;
		}
	}
}


void checkArea2(Object object[], Mat& src, Mat& original, int label[])
{
	for (int i = labelWidth / 10 - 1; i < labelWidth / 10 * 9 + 1; i++)
	{
		for (int j = 0; j < labelHeight / 3 * 2 + 1; j++)
		{
			if (label[j*labelWidth + i] > 0)
			{
				object[label[j*labelWidth + i] - 1].count++;
				object[label[j*labelWidth + i] - 1].centerX += i;
				object[label[j*labelWidth + i] - 1].centerY += j;
				if (object[label[j*labelWidth + i] - 1].checking)
					continue;
				else
				{
					object[label[j*labelWidth + i] - 1].width++;
					object[label[j*labelWidth + i] - 1].checking = true;
				}
			}
		}

		for (int k = 0; k < blackObjectN; k++)
			if (object[k].checking) object[k].checking = false;
	}

	for (int i = 0; i < labelHeight / 3 * 2; i++)
	{
		for (int j = labelWidth / 10 - 1; j < labelWidth / 10 * 9 + 1; j++)
		{
			if (label[i*labelWidth + j] > 0)
			{
				if (object[label[i*labelWidth + j] - 1].checking)
					continue;
				else
				{
					object[label[i*labelWidth + j] - 1].height++;
					object[label[i*labelWidth + j] - 1].checking = true;
				}
			}
		}

		for (int k = 0; k < blackObjectN; k++)
			if (object[k].checking) object[k].checking = false;
	}

	for (int k = 0; k < blackObjectN; k++)
	{
		object[k].centerX /= object[k].count;
		object[k].centerY /= object[k].count;

		if (object[k].count > 2)
		{
			int maxLenght = max(object[k].height, object[k].width);
			int minLenght = min(object[k].height, object[k].width);
			if (maxLenght < minLenght*2)
			{
				object[k].isDeleted = true;
			}
		}
	}
}

void main(void)
{
	/* Setting */
	Mat image = imread("test1.jpg");
	Mat grayScaleImage(image.rows, image.cols, CV_8UC1);
	Mat topHatImage(image.rows, image.cols, CV_8UC1);
	Mat thresholdedImage(image.rows, image.cols, CV_8UC1);
	Mat cannyImage(image.rows, image.cols, CV_8UC1);
	Mat findBlackImage(image.rows, image.cols, CV_8UC1, Scalar(0));
	Mat blackObjectImage(image.rows, image.cols, CV_8UC1, Scalar(0));
	Mat temp(image.rows, image.cols, CV_8UC1, Scalar(0));


	int *label = new int[image.cols*image.rows];
	labelWidth = image.cols;
	labelHeight = image.rows;
	for (int i = 0; i < image.cols*image.rows; i++) label[i] = 0;

	/* GrayScale */
	cvtColor(image, grayScaleImage, CV_BGR2GRAY);

	/* White Top Hat */
	morphologyEx(grayScaleImage, topHatImage, CV_MOP_TOPHAT, element_tophat);

	/* Make Spot Bright and extra color black */
	makeWhite(topHatImage, image, topHatImage);

	/* Thresholding */
	threshold(topHatImage, thresholdedImage, THRESHOLD_LIGHT, 256, CV_THRESH_BINARY);
	
	/* First Filter(Area) */
	objectN = find_components(thresholdedImage, label);
	Object* object = new Object[objectN];
	checkArea(object, thresholdedImage, image, label);

	/* Second Filter(Color) */
	removeDifferColorCandidate(image, object);





	for (int i = 0; i < image.rows; i++)
	{
		for (int j = 0; j < image.cols; j++)
		{
			if (i>image.rows / 3 * 2)
			{
				findBlackImage.at<uchar>(i, j) = 0;
				continue;
			}

			if (j<image.cols / 10 || j>image.cols / 10 * 9)
			{
				findBlackImage.at<uchar>(i, j) = 0;
				continue;
			}

			int blue = image.at<Vec3b>(i,j)[0];
			int green = image.at<Vec3b>(i, j)[1];
			int red = image.at<Vec3b>(i, j)[2];

			double distanceBlack = calcDistance(blue, green, red, 0, 0, 0);

			if (distanceBlack < DISTANCE_REJECT_CANDIDATE_WITH_COLOR)
			{
				if (abs(blue - green) < 30 && abs(green - red) < 30 && abs(blue - red) < 30)
				{
					findBlackImage.at<uchar>(i, j) = 255;
				}
				else
				{
					findBlackImage.at<uchar>(i, j) = 0;
				}
			}
			else
			{
				findBlackImage.at<uchar>(i, j) = 0;
			}
		}
	}


	//int* label2 = new int[image.cols*image.rows];
	//for (int i = 0; i < image.cols*image.rows; i++) label2[i] = 0;

	//blackObjectN = find_components(findBlackImage, label2);
	//Object* blackObject = new Object[objectN];
	//checkArea2(blackObject, thresholdedImage, image, label2);

	//for (int k = 0; k < blackObjectN; k++)
	//{
	//	if (blackObject[k].count < 4)
	//	{
	//		blackObject[k].isDeleted = true;
	//	}

	//	if (blackObject[k].count >= labelWidth * labelHeight * 0.04)
	//	{
	//		blackObject[k].isDeleted = true;
	//	}
	//}

	//for (int k = 0; k < blackObjectN; k++)
	//{
	//	for (int i = 0; i < image.rows; i++)
	//	{
	//		for (int j = 0; j < image.cols; j++)
	//		{
	//			if (!blackObject[k].isDeleted)
	//			{
	//				if (label2[i*image.cols + j] == k + 1)
	//				{
	//					blackObjectImage.at<uchar>(i, j) = 255;
	//				}
	//			}
	//		}
	//	}
	//}


	imshow("image", image);
	imshow("findBlackImage", findBlackImage);
	waitKey(0);

	delete[] label;
	delete[] object;
}

