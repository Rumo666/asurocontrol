#ifndef XQCAMERA_P_H
#define XQCAMERA_P_H

// INCLUDES
#include <e32base.h>
#include <cameraengineobserver.h>
#include <QSize>

// FORWARD DECLARATIONS
class XQCamera;
class CCameraEngine;
class XQViewFinderWidgetPrivate;
class QImage;

// Added by Rick
class asuroqt;

// CLASS DECLARATION
class MVFProcessor
{
public:
      virtual void ViewFinderFrameReady(const QImage& image) = 0;
};

// CLASS DECLARATION
class XQCameraPrivate: public CBase,
    public MCameraEngineObserver
{
public:
    XQCameraPrivate(XQCamera* qq);
    ~XQCameraPrivate();

public:
    bool open(int index);
    void setCaptureSize(QSize size);
    bool capture();
    void close();
    bool focus();
    void cancelFocus();  
    static int camerasAvailable();
    void releaseImageBuffer();
    XQCamera::Error error() const;
    
protected:
    void MceoCameraReady();
    void MceoFocusComplete();
    void MceoCapturedDataReady(TDesC8* aData);
    void MceoCapturedBitmapReady(CFbsBitmap* aBitmap);
    void MceoViewFinderFrameReady(CFbsBitmap& aFrame);
    void MceoHandleError(TCameraEngineError aErrorType, TInt aError);
    
private:
    void setVFProcessor(MVFProcessor* VFProcessor);

private:
    friend class XQViewFinderWidgetPrivate;
    friend class asuroqt; // Added by Rick
    XQCamera* q;
    CCameraEngine* iCameraEngine;
    QSize iViewFinderSize;
    QSize iCaptureSize;
    MVFProcessor* iVFProcessor;
    TInt iIndex;
    mutable int iError;
};

#endif /*XQCAMERA_P_H*/

// End of file
