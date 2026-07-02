#pragma once
#include <vector>

enum class InspectionType  : int { Presence = 0, Color = 1, Defect = 2, Barcode = 3, Measurement = 4 };
enum class AlgorithmEngine : int { RuleBased = 0, DeepLearning = 1 };
enum class LightChannel    : int { White = 0, Red = 1, Green = 2, Blue = 3 };

struct ROI
{
	int             id        = 0;
	CString         name;
	CRect           bounds;              // in golden image pixel coordinates
	InspectionType  type      = InspectionType::Defect;
	AlgorithmEngine engine    = AlgorithmEngine::RuleBased;
	LightChannel    lighting  = LightChannel::White;
	double          tolerance = 0.95;
};

class CPCBModel
{
public:
	CString          name;
	CString          imagePath;
	std::vector<ROI> rois;

	ROI&  AddROI(const CRect& bounds);
	void  RemoveROI(int id);
	ROI*  FindROI(int id);
	void  Clear();

	bool  Save(LPCTSTR path) const;
	bool  Load(LPCTSTR path);

private:
	int m_nextId = 1;
};
