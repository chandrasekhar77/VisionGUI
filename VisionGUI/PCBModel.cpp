#include "pch.h"
#include "framework.h"
#include "PCBModel.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ---------------------------------------------------------------------------
// Model operations
// ---------------------------------------------------------------------------

ROI& CPCBModel::AddROI(const CRect& bounds)
{
	ROI r;
	r.id     = m_nextId++;
	r.name.Format(_T("ROI_%d"), r.id);
	r.bounds = bounds;
	rois.push_back(r);
	return rois.back();
}

void CPCBModel::RemoveROI(int id)
{
	rois.erase(std::remove_if(rois.begin(), rois.end(),
		[id](const ROI& r) { return r.id == id; }), rois.end());
}

ROI* CPCBModel::FindROI(int id)
{
	for (auto& r : rois)
		if (r.id == id) return &r;
	return nullptr;
}

void CPCBModel::Clear()
{
	rois.clear();
	name.Empty();
	imagePath.Empty();
	m_nextId = 1;
}

// ---------------------------------------------------------------------------
// JSON helpers (no external dependency)
// ---------------------------------------------------------------------------

static CString JsonStr(LPCTSTR key, LPCTSTR val, bool last = false)
{
	CString s;
	CString escaped = val;
	escaped.Replace(_T("\\"), _T("\\\\"));
	escaped.Replace(_T("\""), _T("\\\""));
	s.Format(_T("\"%s\":\"%s\"%s"), key, (LPCTSTR)escaped, last ? _T("") : _T(","));
	return s;
}

static CString JsonInt(LPCTSTR key, int val, bool last = false)
{
	CString s;
	s.Format(_T("\"%s\":%d%s"), key, val, last ? _T("") : _T(","));
	return s;
}

static CString JsonDbl(LPCTSTR key, double val, bool last = false)
{
	CString s;
	s.Format(_T("\"%s\":%.4f%s"), key, val, last ? _T("") : _T(","));
	return s;
}

// Extract the first integer value for a given key within a JSON fragment
static int ParseInt(const CString& s, LPCTSTR key)
{
	CString pat; pat.Format(_T("\"%s\":"), key);
	int pos = s.Find(pat);
	if (pos < 0) return 0;
	return _ttoi(s.Mid(pos + pat.GetLength()));
}

static double ParseDbl(const CString& s, LPCTSTR key)
{
	CString pat; pat.Format(_T("\"%s\":"), key);
	int pos = s.Find(pat);
	if (pos < 0) return 0.0;
	return _tstof(s.Mid(pos + pat.GetLength()));
}

static CString ParseStr(const CString& s, LPCTSTR key)
{
	CString pat; pat.Format(_T("\"%s\":\""), key);
	int pos = s.Find(pat);
	if (pos < 0) return _T("");
	int start = pos + pat.GetLength();
	int end   = start;
	while (end < s.GetLength()) {
		if (s[end] == _T('"') && (end == 0 || s[end-1] != _T('\\'))) break;
		end++;
	}
	CString result = s.Mid(start, end - start);
	result.Replace(_T("\\\\"), _T("\\"));
	result.Replace(_T("\\\""), _T("\""));
	return result;
}

// Find the closing brace that matches the opening brace at openPos
static int MatchingBrace(const CString& s, int openPos)
{
	int depth = 0;
	for (int i = openPos; i < s.GetLength(); i++) {
		if (s[i] == _T('{')) depth++;
		else if (s[i] == _T('}') && --depth == 0) return i;
	}
	return -1;
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

bool CPCBModel::Save(LPCTSTR path) const
{
	CStdioFile f;
	if (!f.Open(path, CFile::modeCreate | CFile::modeWrite | CFile::typeText))
		return false;

	f.WriteString(_T("{\n"));
	f.WriteString(_T("  ") + JsonStr(_T("name"), name) + _T("\n"));
	f.WriteString(_T("  ") + JsonStr(_T("imagePath"), imagePath) + _T("\n"));
	f.WriteString(_T("  \"rois\":[\n"));

	for (size_t i = 0; i < rois.size(); i++)
	{
		const ROI& r = rois[i];
		CString bounds;
		bounds.Format(_T("{\"left\":%d,\"top\":%d,\"right\":%d,\"bottom\":%d}"),
			r.bounds.left, r.bounds.top, r.bounds.right, r.bounds.bottom);
		CString line;
		line.Format(_T("    {%s%s\"bounds\":%s,%s%s%s%s}%s\n"),
			(LPCTSTR)JsonStr(_T("name"), r.name),
			(LPCTSTR)JsonInt(_T("id"), r.id),
			(LPCTSTR)bounds,
			(LPCTSTR)JsonInt(_T("type"),     (int)r.type),
			(LPCTSTR)JsonInt(_T("engine"),   (int)r.engine),
			(LPCTSTR)JsonInt(_T("lighting"), (int)r.lighting),
			(LPCTSTR)JsonDbl(_T("tolerance"), r.tolerance, true),
			i < rois.size()-1 ? _T(",") : _T(""));
		f.WriteString(line);
	}

	f.WriteString(_T("  ]\n}\n"));
	return true;
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

bool CPCBModel::Load(LPCTSTR path)
{
	CStdioFile f;
	if (!f.Open(path, CFile::modeRead | CFile::typeText))
		return false;

	CString content, line;
	while (f.ReadString(line))
		content += line + _T("\n");
	f.Close();

	Clear();
	name      = ParseStr(content, _T("name"));
	imagePath = ParseStr(content, _T("imagePath"));

	// Find the rois array
	int roisKey = content.Find(_T("\"rois\""));
	if (roisKey < 0) return true;
	int arrStart = content.Find(_T("["), roisKey);
	if (arrStart < 0) return true;

	int pos = arrStart + 1;
	while (pos < content.GetLength())
	{
		int objStart = content.Find(_T("{"), pos);
		if (objStart < 0) break;
		int objEnd = MatchingBrace(content, objStart);
		if (objEnd < 0) break;

		CString obj = content.Mid(objStart, objEnd - objStart + 1);

		// Extract bounds sub-object
		int bStart = obj.Find(_T("\"bounds\":{"));
		CRect bounds(0,0,0,0);
		if (bStart >= 0) {
			int bObj = obj.Find(_T("{"), bStart + 8);
			int bEnd = MatchingBrace(obj, bObj);
			CString boundsJson = obj.Mid(bObj, bEnd - bObj + 1);
			bounds.left   = ParseInt(boundsJson, _T("left"));
			bounds.top    = ParseInt(boundsJson, _T("top"));
			bounds.right  = ParseInt(boundsJson, _T("right"));
			bounds.bottom = ParseInt(boundsJson, _T("bottom"));
		}

		ROI r;
		r.id        = ParseInt(obj, _T("id"));
		r.name      = ParseStr(obj, _T("name"));
		r.bounds    = bounds;
		r.type      = (InspectionType) ParseInt(obj, _T("type"));
		r.engine    = (AlgorithmEngine)ParseInt(obj, _T("engine"));
		r.lighting  = (LightChannel)  ParseInt(obj, _T("lighting"));
		r.tolerance = ParseDbl(obj, _T("tolerance"));

		rois.push_back(r);
		if (r.id >= m_nextId) m_nextId = r.id + 1;

		pos = objEnd + 1;
	}

	return true;
}
