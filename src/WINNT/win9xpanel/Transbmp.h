/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
// transbmp.h : interface of the CTransBitmap class
//
/////////////////////////////////////////////////////////////////////////////

class CTransBmp : public CBitmap
{
public:
    CTransBmp();
    ~CTransBmp();
    void Draw(HDC hDC, int x, int y);
    void Draw(CDC* pDC, int x, int y);
    void DrawTrans(HDC hDC, CRect &rect,int xoff=0);
    void DrawTrans(CDC* pDC, CRect &rect,int xoff=0);
    int GetWidth();
    int GetHeight();
	void UpdateMask(CWnd *wnd,CRect &rect);
private:
    int m_iWidth;
    int m_iHeight;
    HBITMAP m_hbmMask;    // handle to mask bitmap

    void GetMetrics();
    void CreateMask(HDC hDC, CRect &rect,int xoff);
};

/////////////////////////////////////////////////////////////////////////////
