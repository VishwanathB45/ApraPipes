#include <stdbool.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <gtk/gtk.h>

#include "Logger.h"
#include "GtkGlRenderer.h"
#include "DMAFDWrapper.h"
#include "Background.h"
#include "Matrix.h"
#include "Model.h"
#include "Program.h"
#include "GLUtils.h"
#include "View.h"

struct signal
{
    const gchar *signal;
    GCallback handler;
    GdkEventMask mask;
};

class GtkGlRenderer::Detail
{

public:
    Detail(GtkGlRendererProps &_props) : mProps(_props)
    {
        isMetadataSet = false;
    }

    ~Detail()
    {
    }

    static void
    on_resize(GtkGLArea *area, gint width, gint height, gpointer data)
    {
        printf("In resize width = %d, height = %d\n", width, height);
        view_set_window(width, height);
        background_set_window(width, height);
    }
    void setProps(GtkGlRendererProps &props)
	{
		mProps = props;
	}
    static gboolean
    on_render(GtkGLArea *glarea, GdkGLContext *context, gpointer data)
    {
        //LOG_ERROR<<"DATA IN RENDER "<<data;
        GtkGlRenderer::Detail *detailInstance = (GtkGlRenderer::Detail *)data;

        if (detailInstance->isMetadataSet == false)
        {
            LOG_TRACE << "Metadata is Not Set ";
            return TRUE;
        }
        gint x, y;

    // Check if the child widget is realized (has an associated window)
        if (gtk_widget_get_realized(GTK_WIDGET(glarea))) {
        // Get the immediate parent of the child
            GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(glarea));

        // Check if the parent is realized
            if (parent && gtk_widget_get_realized(parent)) {
            // Get the position of the child relative to its parent
                gtk_widget_translate_coordinates(GTK_WIDGET(glarea), parent, 0, 0, &x, &y);
                // g_print("Child position relative to parent: x=%d, y=%d\n", x, y);
                //LOG_ERROR << "Child position relative to parent "<< x << "====="  << y << "==============" << detailInstance->mProps.windowWidth ;
        } else {
            // g_print("Error: Child's parent is not realized.\n");
        }
    } else {
        // g_print("Error: Child widget is not realized.\n");
    }
        if (!detailInstance->cachedFrame.get())
        {
            LOG_ERROR << "Got Empty Frame";
            return TRUE;
        }
        detailInstance->renderFrame = detailInstance->cachedFrame;
        void *frameToRender;
        if (detailInstance->isDmaMem)
        {
            // frameToRender = static_cast<DMAFDWrapper *>(detailInstance->renderFrame->data())->getCudaPtr();
            frameToRender = static_cast<DMAFDWrapper *>(detailInstance->renderFrame->data())->getHostPtr();
        }
        else
        {
            frameToRender = detailInstance->renderFrame->data();
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Draw background:
        background_draw();

        // Draw model:
        // model_draw();
        // draw_frames();
        drawCameraFrame(frameToRender, detailInstance->frameWidth, detailInstance->frameHeight);


        // Don't propagate signal:
        return TRUE;
    }

    static gboolean
    on_realize(GtkGLArea *glarea, GdkGLContext *context, gpointer data) // Process SOS
    {
        gtk_gl_area_make_current(glarea);

        if (gtk_gl_area_get_error(glarea) != NULL)
        {
            LOG_ERROR << "Failed to initialize buffer";
            return FALSE;
        }
        // Print version info:
        const GLubyte *renderer = glGetString(GL_RENDERER);
        const GLubyte *version = glGetString(GL_VERSION);

        // Enable depth buffer:
        gtk_gl_area_set_has_depth_buffer(glarea, TRUE);

        // Init programs:
        programs_init();

        // Init background:
        background_init();

        // Init model:
        model_init();

        // Get frame clock:
        GdkGLContext *glcontext = gtk_gl_area_get_context(glarea);
        GdkWindow *glwindow = gdk_gl_context_get_window(glcontext);
        GdkFrameClock *frame_clock = gdk_window_get_frame_clock(glwindow);

        // Connect update signal:
        g_signal_connect_swapped(frame_clock, "update", G_CALLBACK(gtk_gl_area_queue_render), glarea);

        // Start updating:
        gdk_frame_clock_begin_updating(frame_clock);

        return TRUE;
    }


    static void on_unrealize(GtkGLArea *glarea, gint width, gint height, gpointer data)
    {
        LOG_ERROR << "UNREALIZE SIGNAL==================================>>>>>>>>>>>>>>>>>";
        // GdkGLContext *glcontext = gtk_gl_area_get_context(GTK_GL_AREA(glarea));
        // GdkWindow *glwindow = gdk_gl_context_get_window(glcontext);
        // GdkFrameClock *frame_clock = gdk_window_get_frame_clock(glwindow);

        // // Disconnect the update signal from frame_clock
        // g_signal_handlers_disconnect_by_func(frame_clock, gtk_gl_area_queue_render, G_OBJECT(glarea));

        // // // Get the parent container
        // GtkWidget *parent_container = gtk_widget_get_parent(glarea);

        // // Remove the GtkGLArea from its parent container
        // gtk_container_remove(GTK_CONTAINER(parent_container), glarea);

        // // Destroy the GtkGLArea widget
        // gtk_widget_destroy(glarea);
    }

    static gboolean
    on_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer data)
    {
        switch (event->direction)
        {
        case GDK_SCROLL_UP:
            view_z_decrease();
            break;

        case GDK_SCROLL_DOWN:
            view_z_increase();
            break;

        default:
            break;
        }

        return FALSE;
    }

    void
    connect_signals(GtkWidget *widget, struct signal *signals, size_t members)
    {
        FOREACH_NELEM(signals, members, s)
        {
            gtk_widget_add_events(widget, s->mask);
            g_signal_connect(widget, s->signal, s->handler, this);
        }
    }

    void
    connect_window_signals(GtkWidget *window)
    {
        struct signal signals[] = {
            {"destroy", G_CALLBACK(gtk_main_quit), (GdkEventMask)0},
        };

        connect_signals(window, signals, NELEM(signals));
    }

    void
    connect_glarea_signals(GtkWidget *glarea)
    {   
        std::chrono::time_point<std::chrono::system_clock> t = std::chrono::system_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch());
        auto timeStamp = dur.count();
        renderId = g_signal_connect(glarea, "render", G_CALLBACK(on_render), this);
        realizeId =  g_signal_connect(glarea, "realize", G_CALLBACK(on_realize), this);
        resizeId = g_signal_connect(glarea, "resize", G_CALLBACK(on_resize), this);
        // g_signal_connect(glarea, "unrealize", G_CALLBACK(on_unrealize), this);
    }

        void disconnect_glarea_signals(GtkWidget *glarea) 
    {
        g_signal_handler_disconnect(glarea, realizeId);
        g_signal_handler_disconnect(glarea, renderId);
        g_signal_handler_disconnect(glarea, resizeId);
    }

    bool init()
    {
        connect_glarea_signals(glarea);
        // initialize_gl(GTK_GL_AREA(glarea));
        return true;
    }

    GtkWidget *glarea;
    int windowWidth, windowHeight;
    uint64_t frameWidth, frameHeight;
    frame_sp cachedFrame, renderFrame;
    void *frameToRender;
    bool isDmaMem;
    bool isMetadataSet;
    GtkGlRendererProps mProps;
    guint realizeId;
    guint renderId;
    guint resizeId;
};

GtkGlRenderer::GtkGlRenderer(GtkGlRendererProps props) : Module(SINK, "GtkGlRenderer", props)
{
    mDetail.reset(new Detail(props));
    mDetail->glarea = props.glArea;
    mDetail->windowWidth = props.windowWidth;
    mDetail->windowHeight = props.windowHeight;
    //LOG_ERROR<<"i am creating gtkgl renderer width and height is "<<mDetail->mProps.windowWidth;
}

GtkGlRenderer::~GtkGlRenderer() {}

bool GtkGlRenderer::init()
{
    if (!Module::init())
    {
        return false;
    }
    if (!mDetail->init())
    {
        LOG_ERROR << "Failed To Initialize GtkGl Area ";
        return false;
    }
    return true;
}

bool GtkGlRenderer::process(frame_container &frames)
{
    for (const auto &pair : frames)
    {
        auto frame = pair.second;
        if (!isFrameEmpty(frame))
        {
            pushFrame(frame);
        }
    }
    processQueue();
    
    return true;
}

void GtkGlRenderer::pushFrame(frame_sp frame)
{
    std::lock_guard<std::mutex> lock(queueMutex);
    frameQueue.push(frame);
}

void GtkGlRenderer::processQueue()
{
    auto currentTime = std::chrono::steady_clock::now();
    auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFrameTime).count();

    std::lock_guard<std::mutex> lock(queueMutex);
    if (!frameQueue.empty())
    {
        auto frame = frameQueue.front();
        frameQueue.pop();
        auto myId = Module::getId();
        if(myId == "GtkGlRenderer_35")
        {
        //    LOG_INFO << "time diff is = " << timeDiff << "Timestamp is = " << frame->timestamp;
        }
       
        if (timeDiff >= 33)
        {
            // LOG_ERROR << "GOT "
            mDetail->cachedFrame = frame;
            size_t underscorePos = myId.find('_');
            std::string numericPart = myId.substr(underscorePos + 1);
            int myNumber = std::stoi(numericPart);

            if ((controlModule != nullptr) && (myNumber % 2 == 1))
            {
                Rendertimestamp cmd;
                auto myTime = frame->timestamp;
                cmd.currentTimeStamp = myTime;
                controlModule->queueCommand(cmd);
                // LOG_ERROR << "myID is GtkGlRendererModule_ "<<myNumber << "sending timestamp "<<myTime;
            }
            lastFrameTime = currentTime;
        }
    }
}

// Need to check on Mem Type Supported
// Already Checked With CPU , Need to check with
// framemetadata_sp metadata = getFirstInputMetadata();
// FrameMetadata::MemType memType = metadata->getMemType();
// if (memType != FrameMetadata::MemType::DMABUF)
// {
// 	LOG_ERROR << "<" << getId() << ">::validateInputPins input memType is expected to be DMABUF. Actual<" << memType << ">";
// 	return false;
// }

bool GtkGlRenderer::validateInputPins()
{
    if (getNumberOfInputPins() < 1)
    {
        LOG_ERROR << "<" << getId() << ">::validateInputPins size is expected to be 1. Actual<" << getNumberOfInputPins() << ">";
        return false;
    }

    return true;
}

bool GtkGlRenderer::term()
{
    bool res = Module::term();
    return res;
}

bool GtkGlRenderer::changeProps(GtkWidget* glArea, int windowWidth, int windowHeight)
{
    //mDetail->on_unrealize();
    mDetail->disconnect_glarea_signals(GTK_WIDGET(mDetail->glarea));
    mDetail->glarea = glArea;
    mDetail->windowWidth = windowWidth;
    mDetail->windowHeight = windowHeight;
    mDetail->init();
    gtk_widget_show(GTK_WIDGET(glArea));
}

bool GtkGlRenderer::shouldTriggerSOS()
{
    if(!mDetail->isMetadataSet)
    {
        LOG_ERROR << "WIll Trigger SOS";
        return true;   
    }
    return false;
}

bool GtkGlRenderer::processSOS(frame_sp &frame)
{   
    //mDetail->connect_glarea_signals(mDetail->glarea);
    auto inputMetadata = frame->getMetadata();
    auto frameType = inputMetadata->getFrameType();
    LOG_TRACE<<"GOT METADATA "<<inputMetadata->getFrameType();
    int width = 0;
    int height = 0;

    switch (frameType)
    {
    case FrameMetadata::FrameType::RAW_IMAGE:
    {
        auto metadata = FrameMetadataFactory::downcast<RawImageMetadata>(inputMetadata);
        if (metadata->getImageType() != ImageMetadata::RGBA )
        {
            throw AIPException(AIP_FATAL, "Unsupported ImageType, Currently Only RGB , BGR , BGRA and RGBA is supported<" + std::to_string(frameType) + ">");
        }
        mDetail->frameWidth = metadata->getWidth();
        mDetail->frameHeight = metadata->getHeight();
        mDetail->isDmaMem = metadata->getMemType() == FrameMetadata::MemType::DMABUF;

        LOG_ERROR << "Width is " << metadata->getWidth() << "Height is " << metadata->getHeight();
        //LOG_ERROR << "Width STEP is " << metadata->
        FrameMetadata::MemType memType = metadata->getMemType();
        {        if (memType != FrameMetadata::MemType::DMABUF)

            LOG_ERROR << "Memory Type Is Not DMA but it's a interleaved Image";
        }
    }
    break;
    case FrameMetadata::FrameType::RAW_IMAGE_PLANAR:
    {
        auto metadata = FrameMetadataFactory::downcast<RawImagePlanarMetadata>(inputMetadata);
        if (metadata->getImageType() != ImageMetadata::RGBA )
        {
            throw AIPException(AIP_FATAL, "Unsupported ImageType, Currently Only RGB, BGR, BGRA and RGBA is supported<" + std::to_string(frameType) + ">");
        }
        mDetail->frameWidth = metadata->getWidth(0);
        mDetail->frameHeight = metadata->getHeight(0);
        mDetail->isDmaMem = metadata->getMemType() == FrameMetadata::MemType::DMABUF;
        LOG_ERROR << "Width is " << metadata->getWidth(0) << "Height is " << metadata->getHeight(0);
        FrameMetadata::MemType memType = metadata->getMemType();
        if (memType != FrameMetadata::MemType::DMABUF)
        {
            LOG_ERROR << "Memory Type Is Not DMA but it's a planar Image";
        }
    }
    break;
    default:
        throw AIPException(AIP_FATAL, "Unsupported FrameType<" + std::to_string(frameType) + ">");
    }
    mDetail->isMetadataSet = true;
    LOG_ERROR << "Done Setting Metadata=========================>";
    // mDetail->init(renderHeight, renderWidth);
    return true;
}

bool GtkGlRenderer::handleCommand(Command::CommandType type, frame_sp &frame)
{
	return Module::handleCommand(type, frame);
}