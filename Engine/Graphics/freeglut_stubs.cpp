extern "C" {
    // Stub pour XParseGeometry
    int XParseGeometry(const char* /*parsestring*/, int* /*x*/, int* /*y*/, 
                       unsigned int* /*width*/, unsigned int* /*height*/) {
        return 0;
    }
    
    void fgSpaceballHandleWinEvent(void* /*msg*/) {}
    
    void fgPlatformInitializeSpaceball(void) {}
    
    void fgPlatformSpaceballClose(void) {}
    
    int fgPlatformHasSpaceball(void) {
        return 0;
    }
    
    int fgPlatformSpaceballNumButtons(void) {
        return 0;
    }
    
    void fgPlatformSpaceballSetWindow(void* /*window*/) {}
}
