#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <Windows.h>
#include "opencv\cv.h"
#include "opencv\highgui.h"
#include <tchar.h>

/* Test mode ON/OFF */
// #define TEST_MODE

#define THRESHOLD 100000000.0 // 일억

#define MAX_NAME_LENGTH 32
#define EIGEN_MATRIX_ROW 12000
#define EIGEN_MATRIX_COL 399

/* Global Variables */

double** eigenfaces;
double** userfaces;
char** usernames;
int usersnumber;
HANDLE hWritePipe;
int newUserMode;

/* Prototypes */

void loadEigenfaces(void);
void loadUserfaces(void);

double* getProjection(double*);
double calEuclideanDistance(double*, double*);

void viewCam(void*);
void viewFace(void*);

IplImage* extractFaceImg(IplImage*);
void faceRecognition(IplImage*);

/////////////////////////////////////////////////////////////////////// Main

int main(int argc, TCHAR *argv[])
{
#ifndef TEST_MODE
	HWND hWnd = GetConsoleWindow();
	ShowWindow(hWnd, SW_HIDE);
#endif
	
	CvCapture* capture = cvCreateCameraCapture(0);

	hWritePipe = (HANDLE)(_ttoi(argv[1]));

	if (argc == 3)
		newUserMode = 1;
	
	// 캠이 없을때
	if (capture == NULL)
	{
		DWORD bytesWritten;
		double nocam[399] = { -1 };

		WriteFile(hWritePipe, nocam, sizeof(double)*399, &bytesWritten, NULL);

		cvReleaseCapture(&capture);
		CloseHandle(hWritePipe);
		return 0;
	}


	_beginthread(viewFace, 0, capture);
	Sleep(500);
	_beginthread(viewCam, 0, capture);

	loadEigenfaces();
	loadUserfaces();
	while (1)
		Sleep(10000);
	cvReleaseCapture(&capture);
	CloseHandle(hWritePipe);

	return 0;

}

/////////////////////////////////////////////////////////////////////// Load Data

void loadEigenfaces(void)
{
	FILE* fp;
	double element;
	double** matrix;
	
	matrix = (double **)malloc(sizeof(double *)* EIGEN_MATRIX_ROW);
	for (int i = 0; i < EIGEN_MATRIX_ROW; i++)
		*(matrix + i) = (double *)malloc(sizeof(double)* EIGEN_MATRIX_COL);

	if ((fp = fopen("Eigenfaces.dat", "r")) == NULL)
		return;

	for (int i = 0; i < EIGEN_MATRIX_ROW; i++) {
		for (int j = 0; j < EIGEN_MATRIX_COL; j++) {
			fscanf(fp, "%lf", &element);
			matrix[i][j] = element;
		}
	}
	fclose(fp);
	eigenfaces = matrix;
}

void loadUserfaces(void)
{
	int c;
	int admin;
	FILE* fp;
	double** matrix;
	char codeMsg[200];

	usersnumber = 0;
	if (userfaces)
		free(userfaces);
	if (usernames)
		free(usernames);
	
	if ((fp = fopen("Userfaces.dat", "rb")) == NULL)
		return;

	while ((c = fgetc(fp)) != EOF) {
		if (c == '\n')
			usersnumber++;
	}

	matrix = (double **)calloc(usersnumber, sizeof(double *));
	for (int i = 0; i < usersnumber; i++)
		*(matrix + i) = (double *)calloc(EIGEN_MATRIX_COL, sizeof(double));

	usernames = (char **)calloc(usersnumber, sizeof(char *));
	for (int i = 0; i < usersnumber; i++)
		*(usernames + i) = (char *)calloc(MAX_NAME_LENGTH, sizeof(char));
	
	fseek(fp, 0, SEEK_SET);

	for (int i = 0; i < usersnumber; i++) {
		fscanf(fp, "%s", usernames[i]);
		fscanf(fp, "%d", &admin);
		for (int j = 0; j < EIGEN_MATRIX_COL; j++) {
			fscanf(fp, "%lf", &matrix[i][j]);
		}
		fscanf(fp, "%s", codeMsg);
		fgetc(fp);
	}

	fclose(fp);
	userfaces = matrix;
}

/////////////////////////////////////////////////////////////////////// Calculation

double calEuclideanDistance(double* vec1, double* vec2)
{
	double sum = 0.0;
	for (int i = 0; i < EIGEN_MATRIX_COL; i++)
		sum += (vec1[i] - vec2[i]) * (vec1[i] - vec2[i]);
	return sqrt(sum);
}

double* getProjection(double *userface)
{
	double sum;
	double* projection = (double *)malloc(sizeof(double)* EIGEN_MATRIX_ROW);

	while (eigenfaces == NULL)
		Sleep(100);

	for (int i = 0; i < EIGEN_MATRIX_COL; i++) {
		sum = 0.0;
		for (int j = 0; j < EIGEN_MATRIX_ROW; j++)
			sum += userface[j] * eigenfaces[j][i];
		projection[i] = sum;
	}

	return projection;
}

/////////////////////////////////////////////////////////////////////// Camera View

void viewCam(void* arg)
{
	int width = GetSystemMetrics(SM_CXSCREEN);
	int height = GetSystemMetrics(SM_CYSCREEN);

	cvNamedWindow("WhoRU", 0);
	cvSetWindowProperty("WhoRU", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);
	cvResizeWindow("WhoRU", 480, 360);
	cvMoveWindow("WhoRU", width / 2 - 240, height / 2 - 180);

	IplImage *frame, *faceImg;
	CvCapture *capture = (CvCapture *)arg;
	
	while (1) {
		frame = cvQueryFrame(capture);
		cvShowImage("WhoRU", frame);
		if (cvWaitKey(33) == ' ') {
			if ((faceImg = extractFaceImg(frame)) != NULL)
				faceRecognition(faceImg);
		}
	}

	cvReleaseImage(&frame);
	cvDestroyWindow("WhoRU");
}

void viewFace(void* arg)
{
	int width = GetSystemMetrics(SM_CXSCREEN);
	int height = GetSystemMetrics(SM_CYSCREEN);

	cvNamedWindow("WhoRU_Face", 0);
	cvSetWindowProperty("WhoRU_Face", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);
	cvResizeWindow("WhoRU_Face", 100, 120);
	cvMoveWindow("WhoRU_Face", width / 2 - 50, height / 2 - 300);

	IplImage *frame, *faceImg;
	CvCapture *capture = (CvCapture *)arg;

	while (1) {
		frame = cvQueryFrame(capture);
		if ((faceImg = extractFaceImg(frame)) != NULL) {
			cvLine(faceImg, cvPoint(faceImg->width / 2, 0), cvPoint(faceImg->width / 2, faceImg->height), CV_RGB(0, 0, 0), 1, 8, 0);
			cvLine(faceImg, cvPoint(0, faceImg->height / 3.3), cvPoint(faceImg->width, faceImg->height / 3.3), CV_RGB(0, 0, 0), 1, 8, 0);
			cvLine(faceImg, cvPoint(0, faceImg->height - (faceImg->height / 4.7)), cvPoint(faceImg->width, faceImg->height - (faceImg->height / 4.7)), CV_RGB(0, 0, 0), 1, 8, 0);
			cvShowImage("WhoRU_Face", faceImg);
			cvReleaseImage(&faceImg);
		}
		cvWaitKey(33);
	}

	cvReleaseImage(&frame);
	cvDestroyWindow("WhoRU_Face");
}

/////////////////////////////////////////////////////////////////////// Face Recognition

IplImage* extractFaceImg(IplImage* frame)
{
	//********** 1. Copy **********//
	IplImage *captureImg = cvCreateImage(cvGetSize(frame), IPL_DEPTH_8U, 3);
	cvCopy(frame, captureImg, captureImg);
	captureImg->origin = frame->origin;

	//********** 2. Face Detection **********//
	CvMemStorage *storage = cvCreateMemStorage(0);
	CvHaarClassifierCascade *cascade = (CvHaarClassifierCascade*)cvLoad("haarcascade_frontalface_alt.xml", 0, 0, 0);
	CvSeq *face = cvHaarDetectObjects(captureImg, cascade, storage, 1.2, 2, CV_HAAR_DO_CANNY_PRUNING, cvSize(0, 0), cvSize(0, 0));

	if (face == NULL || (face != NULL && face->total == 0)) {
		cvReleaseHaarClassifierCascade(&cascade);
		cvReleaseMemStorage(&storage);
		cvReleaseImage(&captureImg);
		return NULL;
	}

	//********** 3. Extract Face Image **********//
	CvRect *r = (CvRect*)cvGetSeqElem(face, 0);
	CvPoint pt1 = { r->x, r->y };
	CvPoint pt2 = { r->x + r->width, r->y + r->height };

	int height = abs(pt2.y - pt1.y) - 10;
	int width = abs(pt2.x - pt1.x) - 40;

	if (height <= 0 || width <= 0) {
		cvReleaseHaarClassifierCascade(&cascade);
		cvReleaseMemStorage(&storage);
		cvReleaseImage(&captureImg);
		return NULL;
	}

	IplImage *faceImg = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
	cvZero(faceImg);

	cvSetImageROI(captureImg, cvRect(pt1.x + 20, pt1.y + 15, width, height));
	cvSetImageROI(faceImg, cvRect(0, 0, width, height));
	cvAdd(captureImg, faceImg, faceImg, NULL);

	faceImg->origin = captureImg->origin;
	
	//********** 4. Release Memory **********//
	cvReleaseHaarClassifierCascade(&cascade);
	cvReleaseMemStorage(&storage);
	cvReleaseImage(&captureImg);

	return faceImg;
}

void faceRecognition(IplImage* faceImg)
{
	//********** 1. Gray **********//
	IplImage* grayImg = cvCreateImage(cvGetSize(faceImg), IPL_DEPTH_8U, 1);
	cvCvtColor(faceImg, grayImg, CV_RGB2GRAY);
	grayImg->origin = faceImg->origin;

	//********** 2. Resizing **********//
	IplImage* resizeImg = cvCreateImage(cvSize(100, 120), IPL_DEPTH_8U, 1);
	cvResize(grayImg, resizeImg, CV_INTER_LINEAR);
	resizeImg->origin = grayImg->origin;

	//********** 3. Conversion **********//
	double *matrix = (double *)calloc(EIGEN_MATRIX_ROW, sizeof(double));
	for (int i = 0; i < EIGEN_MATRIX_ROW; i++)
		matrix[i] = (double)resizeImg->imageData[i];

	//********** 4. Normalization **********//
	double mean, sum = 0.0;
	for (int i = 0; i < EIGEN_MATRIX_ROW; i++)
		sum += matrix[i];
	mean = sum / EIGEN_MATRIX_ROW;
	for (int i = 0; i < EIGEN_MATRIX_ROW; i++)
		matrix[i] -= mean;

	//********** 5. Get Projectioin **********//
	double *temp = matrix;
	matrix = getProjection(temp);
	free(temp);

	//********** 6. Face Recognition **********//
	if (newUserMode) {
		DWORD bytesWritten;
		WriteFile(hWritePipe, matrix, sizeof(double) * 399, &bytesWritten, NULL);
	} else {
		int minIndex;
		double minValue;
		double val;

		while (userfaces == NULL)
			Sleep(100);

		if (usersnumber < 1)
			return;

		minIndex = 0;
		minValue = calEuclideanDistance(matrix, userfaces[0]);
		for (int i = 0; i < usersnumber; i++) {
			val = calEuclideanDistance(matrix, userfaces[i]);
			printf("[%2d %10s] %lf\n", i, usernames[i], val);
			if (val < minValue) {
				minValue = val;
				minIndex = i;
			}
		}
		
		if (THRESHOLD < minValue) {
			printf(">> WhoRU? Unknown user\n\n");
			minIndex = -1;
		} else {
			printf(">> WhoRU? [%2d %10s]\n\n", minIndex, usernames[minIndex]);
		}
		DWORD bytesWritten;
		TCHAR sendString[10];

		_stprintf(sendString, _T("%d"), minIndex);

		WriteFile(hWritePipe, sendString, 10, &bytesWritten, NULL);
	}

	//********** 7. Release Memory **********//
	cvReleaseImage(&faceImg);
	cvReleaseImage(&grayImg);
	cvReleaseImage(&resizeImg);
}