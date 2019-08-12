#pragma once
#include "stdafx.h"
#include "ArcFaceEngine.h"
#include <string>
#include <mutex>

using namespace std;

#define NSCALE 32
#define FACENUM 5

ArcFaceEngine::ArcFaceEngine()
{

}

ArcFaceEngine::~ArcFaceEngine()
{

}



MRESULT ArcFaceEngine::ActiveSDK(char* appID,char* sdkKey, char* activeKey)
{
#ifdef PRO
	MRESULT res = ASFOnlineActivation(appID, sdkKey, activeKey);
#else 
	MRESULT res = ASFOnlineActivation(appID, sdkKey);
#endif
	if (MOK != res && MERR_ASF_ALREADY_ACTIVATED != res)
		return res;
	return MOK;
}

MRESULT ArcFaceEngine::GetActiveFileInfo(ASF_ActiveFileInfo& activeFileInfo)
{
	MRESULT res = ASFGetActiveFileInfo(&activeFileInfo);
	return res;
}

MRESULT ArcFaceEngine::InitEngine(MLong detectMode)
{
	m_hEngine = NULL;
	MInt32 mask = 0;

	if (ASF_DETECT_MODE_IMAGE == detectMode)
	{
		mask = ASF_FACE_DETECT | ASF_FACERECOGNITION | ASF_AGE | ASF_GENDER | ASF_FACE3DANGLE | ASF_LIVENESS;
	}
	else
	{
		mask = ASF_FACE_DETECT | ASF_FACERECOGNITION | ASF_LIVENESS | ASF_IR_LIVENESS;
	}

	//2.2版本IMAGE模式下不支持全角度
	MRESULT res = ASFInitEngine(detectMode, ASF_OP_0_ONLY, NSCALE, FACENUM, mask, &m_hEngine);
	return res;
}


MRESULT ArcFaceEngine::FacePairMatching(MFloat &confidenceLevel, ASF_FaceFeature feature1, ASF_FaceFeature feature2)
{
	int res = ASFFaceFeatureCompare(m_hEngine, &feature1, &feature2, &confidenceLevel);
	return res;
}

MRESULT ArcFaceEngine::SetLivenessThreshold(MFloat	rgbLiveThreshold, MFloat irLiveThreshold)
{
	ASF_LivenessThreshold threshold = { 0 };
	threshold.thresholdmodel_BGR = rgbLiveThreshold;
	threshold.thresholdmodel_IR = irLiveThreshold;

	int res = ASFSetLivenessParam(m_hEngine, &threshold);
	return res;
}

MRESULT ArcFaceEngine::FaceASFProcess(ASF_MultiFaceInfo detectedFaces, IplImage *img, ASF_AgeInfo &ageInfo,
	ASF_GenderInfo &genderInfo, ASF_Face3DAngle &angleInfo, ASF_LivenessInfo& liveNessInfo)
{
	if (!img)
	{
		return -1;
	}

	MInt32 lastMask = ASF_AGE | ASF_GENDER | ASF_FACE3DANGLE | ASF_LIVENESS;

	IplImage* cutImg = cvCreateImage(cvSize(img->width - (img->width % 4), img->height), IPL_DEPTH_8U, img->nChannels);

	PicCutOut(img, cutImg, 0, 0);

	if (!cutImg)
	{
		cvReleaseImage(&cutImg);
		return -1;
	}

	int res = ASFProcess(m_hEngine, cutImg->width, cutImg->height, ASVL_PAF_RGB24_B8G8R8,
		(MUInt8*)cutImg->imageData, &detectedFaces, lastMask);

	res = ASFGetAge(m_hEngine, &ageInfo);

	res = ASFGetGender(m_hEngine, &genderInfo);

	res = ASFGetFace3DAngle(m_hEngine, &angleInfo);

	res = ASFGetLivenessScore(m_hEngine, &liveNessInfo);

	cvReleaseImage(&cutImg);

	return res;
}

MRESULT ArcFaceEngine::FaceASFProcess_IR(ASF_MultiFaceInfo detectedFaces, IplImage *img, ASF_LivenessInfo& irLiveNessInfo)
{
	if (!img)
	{
		return -1;
	}

	cv::Mat grayMat;
	cv::Mat matImg(img, false);						//IplImage转Mat 设为ture为深拷贝
	cv::cvtColor(matImg, grayMat, CV_BGR2GRAY);
	IplImage* iplGrayMat = &IplImage(grayMat);		//mat 转 IplImage 浅拷贝

	MInt32 lastMask = ASF_IR_LIVENESS;

	IplImage* cutGrayImg = cvCreateImage(cvSize(iplGrayMat->width - (iplGrayMat->width % 4), iplGrayMat->height), IPL_DEPTH_8U, iplGrayMat->nChannels);

	PicCutOut(iplGrayMat, cutGrayImg, 0, 0);

	if (!cutGrayImg)
	{
		cvReleaseImage(&cutGrayImg);
		return -1;
	}

	int res = ASFProcess_IR(m_hEngine, cutGrayImg->width, cutGrayImg->height, ASVL_PAF_GRAY,
		(MUInt8*)cutGrayImg->imageData, &detectedFaces, lastMask);

	res = ASFGetLivenessScore(m_hEngine, &irLiveNessInfo);

	cvReleaseImage(&cutGrayImg);

	return res;
}

const ASF_VERSION* ArcFaceEngine::GetVersion()
{
	const ASF_VERSION* pVersionInfo = ASFGetVersion(m_hEngine);
	return pVersionInfo;
}

MRESULT ArcFaceEngine::PreDetectFace(IplImage* image, ASF_SingleFaceInfo& faceRect, bool isRGB)
{
	if (!image)
	{
		return -1;
	}

	IplImage* cutImg = NULL;
	MRESULT res = MOK;
	ASF_MultiFaceInfo detectedFaces = { 0 };//人脸检测
	
	if (isRGB)
	{
		cutImg = cvCreateImage(cvSize(image->width - (image->width % 4), image->height),
			IPL_DEPTH_8U, image->nChannels);

		PicCutOut(image, cutImg, 0, 0);

		res = ASFDetectFaces(m_hEngine, cutImg->width, cutImg->height,
			ASVL_PAF_RGB24_B8G8R8, (MUInt8*)cutImg->imageData, &detectedFaces);
	}
	else  //IR图像
	{
		cv::Mat grayMat;
		cv::Mat matImg(image, false);						//IplImage转Mat 设为ture为深拷贝
		cv::cvtColor(matImg, grayMat, CV_BGR2GRAY);
		IplImage* iplGrayMat = &IplImage(grayMat);		//mat 转 IplImage 浅拷贝

		cutImg = cvCreateImage(cvSize(iplGrayMat->width - (iplGrayMat->width % 4), iplGrayMat->height),
			IPL_DEPTH_8U, iplGrayMat->nChannels);

		PicCutOut(iplGrayMat, cutImg, 0, 0);

		res = ASFDetectFaces(m_hEngine, cutImg->width, cutImg->height,
			ASVL_PAF_GRAY, (MUInt8*)cutImg->imageData, &detectedFaces);
	}

	if (res != MOK || detectedFaces.faceNum < 1)
	{
		cvReleaseImage(&cutImg);
		return -1;
	}

	int max = 0;
	int maxArea = 0;

	for (int i = 0; i < detectedFaces.faceNum; i++)
	{
		if (detectedFaces.faceRect[i].left < 0)
			detectedFaces.faceRect[i].left = 10;
		if (detectedFaces.faceRect[i].top < 0)
			detectedFaces.faceRect[i].top = 10;
		if (detectedFaces.faceRect[i].right < 0 || detectedFaces.faceRect[i].right > cutImg->width)
			detectedFaces.faceRect[i].right = cutImg->width - 10;
		if (detectedFaces.faceRect[i].bottom < 0 || detectedFaces.faceRect[i].bottom > cutImg->height)
			detectedFaces.faceRect[i].bottom = cutImg->height - 10;

		if ((detectedFaces.faceRect[i].right - detectedFaces.faceRect[i].left)*
			(detectedFaces.faceRect[i].bottom - detectedFaces.faceRect[i].top) > maxArea)
		{
			max = i;
			maxArea = (detectedFaces.faceRect[i].right - detectedFaces.faceRect[i].left)*
				(detectedFaces.faceRect[i].bottom - detectedFaces.faceRect[i].top);
		}
	}

	faceRect.faceRect.left = detectedFaces.faceRect[max].left;
	faceRect.faceRect.top = detectedFaces.faceRect[max].top;
	faceRect.faceRect.right = detectedFaces.faceRect[max].right;
	faceRect.faceRect.bottom = detectedFaces.faceRect[max].bottom;
	faceRect.faceOrient = detectedFaces.faceOrient[max];
	cvReleaseImage(&cutImg);

	return res;
}


// 预先提取人脸特征
MRESULT ArcFaceEngine::PreExtractFeature(IplImage* image, ASF_FaceFeature& feature, ASF_SingleFaceInfo& faceRect)
{
	if (!image || image->imageData == NULL)
	{
		return -1;
	}
	IplImage* cutImg = cvCreateImage(cvSize(image->width - (image->width % 4), image->height),
		IPL_DEPTH_8U, image->nChannels);

	PicCutOut(image, cutImg, 0, 0);

	if (!cutImg)
	{
		cvReleaseImage(&cutImg);
		return -1;
	}

	MRESULT res = MOK;

	ASF_FaceFeature detectFaceFeature = { 0 };//特征值

	res = ASFFaceFeatureExtract(m_hEngine, cutImg->width, cutImg->height, ASVL_PAF_RGB24_B8G8R8,
		(MUInt8*)cutImg->imageData, &faceRect, &detectFaceFeature);

	if (MOK != res)
	{
		cvReleaseImage(&cutImg);
		return res;
	}

	if (!feature.feature)
	{
		return -1;
	}
	memset(feature.feature, 0, detectFaceFeature.featureSize);
	memcpy(feature.feature, detectFaceFeature.feature, detectFaceFeature.featureSize);
	cvReleaseImage(&cutImg);

	return res;
}


MRESULT ArcFaceEngine::UnInitEngine()
{
	//销毁引擎
	return ASFUninitEngine(m_hEngine);
}

void PicCutOut(IplImage* src, IplImage* dst, int x, int y)
{
	if (!src || !dst)
	{
		return;
	}

	CvSize size = cvSize(dst->width, dst->height);
	cvSetImageROI(src, cvRect(x, y, size.width, size.height));
	cvCopy(src, dst);
	cvResetImageROI(src);
	src = dst;
}