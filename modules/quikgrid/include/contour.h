void Contour( SurfaceGrid &Zgrid, float ContourValue );

typedef void (*DoLineToPtr) ( float x, float y, int drawtype );
extern DoLineToPtr DoLineTo;