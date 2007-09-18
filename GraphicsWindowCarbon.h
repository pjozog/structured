/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield 
 *
 * This library is open source and may be redistributed and/or modified under  
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or 
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * OpenSceneGraph Public License for more details.
*/

/* Note, elements of GraphicsWindowX11 have used Prodcer/RenderSurface_X11.cpp as both
 * a guide to use of X11/GLX and copiying directly in the case of setBorder().
 * These elements are license under OSGPL as above, with Copyright (C) 2001-2004  Don Burns.
 */

#ifndef OSGVIEWER_GRAPHICSWINDOWCARBON
#define OSGVIEWER_GRAPHICSWINDOWCARBON 1

#ifdef __APPLE__

#include <osgViewer/GraphicsWindow>
#include <Carbon/Carbon.h>
#include <AGL/agl.h>
namespace osgViewer
{

class GraphicsWindowCarbon : public osgViewer::GraphicsWindow
{
    public:

        GraphicsWindowCarbon(osg::GraphicsContext::Traits* traits):
            _valid(false),
            _initialized(false),
            _realized(false),
            _ownsWindow(true)
        {
            _traits = traits;

            init();
            
            if (valid())
            {
                setState( new osg::State );
                getState()->setGraphicsContext(this);

                if (_traits.valid() && _traits->sharedContext)
                {
                    getState()->setContextID( _traits->sharedContext->getState()->getContextID() );
                    incrementContextIDUsageCount( getState()->getContextID() );   
                }
                else
                {
                    getState()->setContextID( osg::GraphicsContext::createNewContextID() );
                }
            }
        }
    
        virtual bool isSameKindAs(const Object* object) const { return dynamic_cast<const GraphicsWindowCarbon*>(object)!=0; }
        virtual const char* libraryName() const { return "osgViewer"; }
        virtual const char* className() const { return "GraphicsWindowCarbon"; }

        virtual bool valid() const { return _valid; }

        /** Realise the GraphicsContext.*/
        virtual bool realizeImplementation();

        /** Return true if the graphics context has been realised and is ready to use.*/
        virtual bool isRealizedImplementation() const { return _realized; }

        /** Close the graphics context.*/
        virtual void closeImplementation();

        /** Make this graphics context current.*/
        virtual bool makeCurrentImplementation();
        
        /** Release the graphics context.*/
        virtual bool releaseContextImplementation();

        /** Swap the front and back buffers.*/
        virtual void swapBuffersImplementation();
        
        /** Check to see if any events have been generated.*/
        virtual void checkEvents();

        /** Set the window's position and size.*/
        virtual bool setWindowRectangleImplementation(int x, int y, int width, int height);

        /** Set Window decoration.*/
        virtual bool setWindowDecorationImplementation(bool flag);

        /** Get focus.*/
        virtual void grabFocus();
        
        /** Get focus on if the pointer is in this window.*/
        virtual void grabFocusIfPointerInWindow();
        
        void requestClose() { _closeRequested = true; }
        
        virtual void resizedImplementation(int x, int y, int width, int height);
        
        WindowRef getNativeWindowRef() { return _window; }
        
        bool handleMouseEvent(EventRef theEvent);
        bool handleKeyboardEvent(EventRef theEvent);

        /** WindowData is used to pass in the Carbon window handle attached the GraphicsContext::Traits structure. */
        class WindowData : public osg::Referenced
        {
            public:
                WindowData(WindowRef window):
                    _window(window), _installEventHandler(false) {}
                
                WindowRef getNativeWindowRef() { return _window; }
                void setInstallEventHandler(bool flag) { _installEventHandler = flag; }
                bool installEventHandler() { return _installEventHandler; }
            
            private:
                WindowRef   _window;
                bool        _installEventHandler;
            
        };
        
        /// install the standard os-eventhandler
        void installEventHandler();
        
        /// get the AGL context
        AGLContext getAGLContext() { return _context; }
        
        // get the pixelformat
        AGLPixelFormat getAGLPixelFormat() { return _pixelFormat; }
		

    protected:
    
        void init();
        
        void transformMouseXY(float& x, float& y);
        
        
        


        bool            _valid;
        bool            _initialized;
        bool            _realized;
        bool            _useWindowDecoration;

        bool            _ownsWindow;
        WindowRef       _window;
        AGLContext      _context;
        AGLPixelFormat  _pixelFormat;
        
        int             _windowTitleHeight;    
    private:        
        /// computes the window attributes
        WindowAttributes computeWindowAttributes(bool useWindowDecoration, bool supportsResize);
    
        
        bool _closeRequested;
};

}

#endif
#endif
