// ===== BLOCK 0: MODE SELECT =====
//#define USE_VIEWER

// ===== BLOCK 0B: WINDOWS HEADER =====
#include <windows.h>
#include <windowsx.h>

// ===== BLOCK 1: STANDARD LIBRARY =====
#include <iostream>
#include <string>
#include <map>
#include <sstream>
#include <iomanip>

// ===== BLOCK 2: OCCT GEOMETRY KERNEL =====
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <TopoDS_Shape.hxx>
#include <TopExp_Explorer.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

// ===== BLOCK 2.5: STEP IMPORT / EXPORT =====
#include <STEPControl_Reader.hxx>

// ===== BLOCK 2.6: RAPIDJSON =====
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

// ===== BLOCK 3: OCCT VISUALIZATION – PROFESSIONAL =====
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <V3d_Viewer.hxx>
#include <V3d_View.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <WNT_Window.hxx>
#include <Aspect_GradientBackground.hxx>
#include <Graphic3d_MaterialAspect.hxx>
#include <Graphic3d_RenderingParams.hxx>
#include <Graphic3d_TextureEnv.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_ColorRGBA.hxx>
#include <V3d_AmbientLight.hxx>
#include <V3d_DirectionalLight.hxx>
#include <V3d_PositionalLight.hxx>
#include <V3d_SpotLight.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <Prs3d_PointAspect.hxx>
#include <Aspect_Window.hxx>

// ===== BLOCK 4: VIEWER STATE =====
struct ViewerState
{
    Handle(V3d_View)              view;
    Handle(AIS_InteractiveContext) context;
    bool   isRotating = false;
    bool   isPanning  = false;
    bool   isZooming  = false;
    int    startX = 0, startY = 0;        // used for panning
    int    zoomStartX = 0, zoomStartY = 0; // used for right‑drag zoom
    Standard_Real startScale = 1.0;        // kept for compatibility (not used for zoom now)
    Standard_Real currentZoom = 1.0;       // cumulative zoom for mouse wheel
    bool   showEdges = true;
    int    displayMode = 0;
};

// ===== BLOCK 4.5: SHAPE REGISTRY =====
std::map<std::string, TopoDS_Shape> g_shapeMap;

// ===== BLOCK 4.6: COUNT TOPOLOGY FOR HUD =====
static void CountTopology(const TopoDS_Shape& shape, int& nFaces, int& nEdges, int& nVertices)
{
    nFaces = nEdges = nVertices = 0;
    for (TopExp_Explorer ex(shape, TopAbs_FACE);   ex.More(); ex.Next()) nFaces++;
    for (TopExp_Explorer ex(shape, TopAbs_EDGE);   ex.More(); ex.Next()) nEdges++;
    for (TopExp_Explorer ex(shape, TopAbs_VERTEX); ex.More(); ex.Next()) nVertices++;
}

// ===== BLOCK 5: PROFESSIONAL WINDOW PROCEDURE =====
LRESULT CALLBACK ViewerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ViewerState* state = reinterpret_cast<ViewerState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!state || !state->view)
        return DefWindowProcW(hwnd, msg, wParam, lParam);

    Handle(V3d_View)&              view = state->view;
    Handle(AIS_InteractiveContext)& ctx  = state->context;
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);

    switch (msg)
    {
    case WM_SIZE:
        if (!view->Window().IsNull()) view->MustBeResized();
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        if (!view->Window().IsNull()) view->Redraw();
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;   // no flicker

    // ---- Mouse buttons ----
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        state->isRotating = true;
        view->StartRotation(x, y);
        break;

    case WM_MBUTTONDOWN:
        SetCapture(hwnd);
        state->isPanning = true;
        state->startX = x;
        state->startY = y;
        view->Panning(0, 0, 1, Standard_True);
        break;

    case WM_RBUTTONDOWN:
        SetCapture(hwnd);
        state->isZooming = true;
        state->zoomStartX = x;      // store start point for ZoomAtPoint
        state->zoomStartY = y;
        view->StartZoomAtPoint(x, y);   // OCCT internal reference
        break;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
        ReleaseCapture();
        state->isRotating = false;
        state->isPanning  = false;
        break;

    case WM_RBUTTONUP:
        ReleaseCapture();
        state->isZooming = false;
        // After a drag zoom, synchronise the cumulative zoom from the view
        state->currentZoom = view->Scale();
        break;

    case WM_MOUSEMOVE:
        if (state->isRotating)
        {
            view->Rotation(x, y);
        }
        else if (state->isPanning)
        {
            Standard_Real dx = static_cast<Standard_Real>(x - state->startX);
            Standard_Real dy = static_cast<Standard_Real>(y - state->startY);
            view->Panning(dx, dy, 1, Standard_False);
        }
        else if (state->isZooming)
        {
            // Right‑drag zoom: uses OCCT's precise ZoomAtPoint
            view->ZoomAtPoint(state->zoomStartX, state->zoomStartY, x, y);
        }
        break;

    case WM_MOUSEWHEEL:
    {
        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        // Wheel up = zoom in (×1.15), down = zoom out (÷1.15 = 0.8696)
        Standard_Real factor = (zDelta > 0) ? 1.15 : (1.0 / 1.15);
        state->currentZoom *= factor;
        view->SetZoom(state->currentZoom);
        break;
    }

    // ---- Keyboard shortcuts ----
    case WM_KEYDOWN:
        switch (wParam)
        {
        case 'F': view->FitAll(); state->currentZoom = view->Scale(); break;

        // Standard views
        case 'T': view->SetProj(V3d_Xpos); break;            // Top
        case 'B': view->SetProj(V3d_Xneg); break;            // Bottom
        case 'R': view->SetProj(V3d_Ypos); break;            // Right
        case 'L': view->SetProj(V3d_Yneg); break;            // Left
        case 'K': view->SetProj(V3d_Zpos); break;            // Back
        case 'G': view->SetProj(V3d_Zneg); break;            // Front
        case 'I': view->SetProj(V3d_XposYnegZpos); break;    // Isometric

        // Display modes
        case 'W':   // Wireframe
            ctx->SetDisplayMode(AIS_WireFrame, Standard_True);
            state->displayMode = 1;
            break;

        case 'S':   // Shaded only
            ctx->SetDisplayMode(AIS_Shaded, Standard_True);
            state->displayMode = 0;
            break;

        case 'E':   // Shaded + visible edges
            ctx->SetDisplayMode(AIS_Shaded, Standard_True);
            state->showEdges = !state->showEdges;
            ctx->DisplayAll(Standard_True);
            state->displayMode = 2;
            break;

        case VK_ESCAPE:
            DestroyWindow(hwnd);
            break;

        case VK_F5:   // Screenshot
            if (!view->Window().IsNull())
                view->Dump("vison_screenshot.png");
            break;

        case VK_F11:  // Force redraw
            if (!view->Window().IsNull())
            {
                view->Redraw();
                view->MustBeResized();
            }
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ===== BLOCK 6: WINDOW CREATION =====
HWND CreateNativeWindow(HINSTANCE hInstance, int nCmdShow, const std::string& title)
{
    const wchar_t CLASS_NAME[] = L"VisonCAD_Viewer";
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = ViewerWndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.style         = CS_OWNDC;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;   // OCCT paints it all
    RegisterClassW(&wc);

    // Convert title to wide string
    std::wstring wtitle(title.begin(), title.end());

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, wtitle.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1600, 1000,
        nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow);
    return hwnd;
}

// ===== BLOCK 6.5: PROFESSIONAL VIEWER LAUNCH =====
void ShowShapeInViewer(const std::string& id)
{
    auto it = g_shapeMap.find(id);
    if (it == g_shapeMap.end())
    {
        std::cerr << "[Viewer] Shape '" << id << "' not found.\n";
        return;
    }

    const TopoDS_Shape& shape = it->second;

    // ---- Compute topology for HUD ----
    int totalFaces, totalEdges, totalVertices;
    CountTopology(shape, totalFaces, totalEdges, totalVertices);

    // ---- Graphics driver ----
    Handle(Graphic3d_GraphicDriver) gfxDrv = new OpenGl_GraphicDriver(nullptr);

    // ---- 3D Viewer (scene manager) ----
    Handle(V3d_Viewer) viewer = new V3d_Viewer(gfxDrv);

    // ---- Rendering params ----
    Graphic3d_RenderingParams rp;
    rp.IsAntialiasingEnabled = Standard_True;
    rp.NbMsaaSamples         = 8;
    viewer->SetDefaultRenderingParams(rp);

    // ===== LIGHTING SETUP =====
    viewer->SetDefaultLights();   // basic headlight + ambient
    viewer->SetLightOn();

    // Ambient light – soft overall illumination
    Handle(V3d_AmbientLight) ambLight = new V3d_AmbientLight(
        Quantity_Color(0.30, 0.30, 0.30, Quantity_TOC_RGB));
    viewer->SetLightOn(ambLight);

    // Key directional light – main shape-defining light
    Handle(V3d_DirectionalLight) keyLight = new V3d_DirectionalLight(
        V3d_XnegYnegZpos,
        Quantity_Color(0.95, 0.95, 0.90, Quantity_TOC_RGB),
        Standard_True);
    viewer->SetLightOn(keyLight);

    // Fill directional light – reduces harsh shadows
    Handle(V3d_DirectionalLight) fillLight = new V3d_DirectionalLight(
        V3d_XposYposZpos,
        Quantity_Color(0.50, 0.50, 0.55, Quantity_TOC_RGB),
        Standard_True);
    viewer->SetLightOn(fillLight);

    // Headlight – always from camera
    Handle(V3d_DirectionalLight) headLight = new V3d_DirectionalLight(
        V3d_Zneg,
        Quantity_Color(0.40, 0.40, 0.40, Quantity_TOC_RGB),
        Standard_True);
    headLight->SetHeadlight(Standard_True);
    viewer->SetLightOn(headLight);

    // Rim light – highlights edges / contours
    Handle(V3d_DirectionalLight) rimLight = new V3d_DirectionalLight(
        V3d_XposYnegZneg,
        Quantity_Color(0.35, 0.35, 0.40, Quantity_TOC_RGB),
        Standard_True);
    viewer->SetLightOn(rimLight);

    // ---- Gradient Background ----
    viewer->SetDefaultBgGradientColors(
        Quantity_Color(0.08, 0.08, 0.14, Quantity_TOC_RGB),   // dark blue-gray at top
        Quantity_Color(0.28, 0.28, 0.38, Quantity_TOC_RGB),   // lighter at bottom
        Aspect_GradientFillMethod_Vertical);

    // ---- Window ----
    std::string title = "Vison CAD — " + id +
                        "  [Faces:" + std::to_string(totalFaces) +
                        " Edges:" + std::to_string(totalEdges) +
                        " Verts:" + std::to_string(totalVertices) + "]";
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    HWND hwnd = CreateNativeWindow(hInst, SW_SHOW, title);
    if (!hwnd) return;

    Handle(WNT_Window) wntWin = new WNT_Window(hwnd);

    // ---- View (camera) ----
    Handle(V3d_View) view = viewer->CreateView();
    view->SetWindow(wntWin);
    if (!wntWin->IsMapped()) wntWin->Map();
    view->MustBeResized();

    // Isometric default
    view->SetProj(V3d_XposYnegZpos);
    // Trihedron (coordinate axes in corner)
    view->TriedronDisplay(Aspect_TOTP_RIGHT_LOWER, Quantity_NOC_WHITE, 0.08);

    // ---- Interactive context ----
    Handle(AIS_InteractiveContext) ctx = new AIS_InteractiveContext(viewer);
    ctx->SetDisplayMode(AIS_Shaded, Standard_True);
    ctx->SetAutoActivateSelection(Standard_False);

    // ===== SHAPE DISPLAY =====
    Handle(AIS_Shape) aisShape = new AIS_Shape(shape);

    // Material – polished aluminium
    Graphic3d_MaterialAspect mat(Graphic3d_NOM_ALUMINIUM);
    mat.SetShininess(0.65);
    mat.SetRefractionIndex(1.5);
    aisShape->SetMaterial(mat);

    // Color – neutral blue-gray, professional look
    Quantity_ColorRGBA color(0.70, 0.75, 0.82, 1.0);
    aisShape->SetColor(color.GetRGB());

    // Edges – thin dark lines for crisp detail
    Handle(Prs3d_Drawer) drawer = aisShape->Attributes();
    Handle(Prs3d_LineAspect) edgeAspect = drawer->LineAspect();
    edgeAspect->SetColor(Quantity_NOC_BLACK);
    edgeAspect->SetWidth(1.0);
    edgeAspect->SetTypeOfLine(Aspect_TOL_SOLID);
    drawer->SetFaceBoundaryDraw(Standard_True);
    drawer->SetFaceBoundaryAspect(edgeAspect);

    // Vertex display – small dots (useful for debugging)
    Handle(Prs3d_PointAspect) ptAspect = drawer->PointAspect();
    ptAspect->SetTypeOfMarker(Aspect_TOM_POINT);
    ptAspect->SetScale(2.0);

    ctx->Display(aisShape, Standard_True);
    view->FitAll();

    // ---- Viewer state for WndProc ----
    ViewerState state;
    state.view    = view;
    state.context = ctx;
    state.showEdges = true;
    state.displayMode = 0;
    state.currentZoom = view->Scale();   // initialise after FitAll
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));

    // ---- Message loop ----
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// ===== BLOCK 7: HEADLESS JSON PROCESSOR =====
static void addShapeMetadata(rapidjson::Document& doc, const TopoDS_Shape& shape,
                             rapidjson::Document::AllocatorType& alloc)
{
    using namespace rapidjson;
    int nFaces = 0, nEdges = 0, nVertices = 0;
    CountTopology(shape, nFaces, nEdges, nVertices);

    Value meta(kObjectType);
    meta.AddMember("faces",    nFaces,    alloc);
    meta.AddMember("edges",    nEdges,    alloc);
    meta.AddMember("vertices", nVertices, alloc);
    doc.AddMember("metadata", meta, alloc);

    // Bounding box
    Bnd_Box bbox;
    BRepBndLib::Add(shape, bbox);
    if (!bbox.IsVoid())
    {
        Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
        bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        Value bboxVal(kObjectType);
        bboxVal.AddMember("xmin", xmin, alloc);
        bboxVal.AddMember("ymin", ymin, alloc);
        bboxVal.AddMember("zmin", zmin, alloc);
        bboxVal.AddMember("xmax", xmax, alloc);
        bboxVal.AddMember("ymax", ymax, alloc);
        bboxVal.AddMember("zmax", zmax, alloc);
        doc.AddMember("bbox", bboxVal, alloc);
    }
}

void ProcessJsonInstruction(const std::string& jsonStr,
                           std::map<std::string, TopoDS_Shape>& shapeMap)
{
    using namespace rapidjson;
    Document doc;
    doc.Parse(jsonStr.c_str());

    Document responseDoc;
    responseDoc.SetObject();
    Document::AllocatorType& alloc = responseDoc.GetAllocator();

    if (doc.HasParseError())
    {
        responseDoc.AddMember("status", "error", alloc);
        std::string errMsg = "JSON parse error at offset " +
                             std::to_string(doc.GetErrorOffset());
        responseDoc.AddMember("message", Value(errMsg.c_str(), alloc), alloc);
    }
    else if (!doc.IsObject() || !doc.HasMember("operation"))
    {
        responseDoc.AddMember("status", "error", alloc);
        responseDoc.AddMember("message", "Missing or invalid 'operation' field", alloc);
    }
    else
    {
        std::string operation = doc["operation"].GetString();
        std::string id = doc.HasMember("id") ? doc["id"].GetString() : "unnamed";

        TopoDS_Shape resultShape;
        bool ok = false;

        if (operation == "make_box")
        {
            double w = doc.HasMember("width")  ? doc["width"].GetDouble()  : 100.0;
            double d = doc.HasMember("depth")  ? doc["depth"].GetDouble()  : 100.0;
            double h = doc.HasMember("height") ? doc["height"].GetDouble() : 100.0;
            BRepPrimAPI_MakeBox maker(w, d, h);
            resultShape = maker.Shape();
            ok = !resultShape.IsNull();
        }
        else if (operation == "make_cylinder")
        {
            double r = doc.HasMember("radius") ? doc["radius"].GetDouble() : 30.0;
            double h = doc.HasMember("height") ? doc["height"].GetDouble() : 100.0;
            BRepPrimAPI_MakeCylinder maker(r, h);
            resultShape = maker.Shape();
            ok = !resultShape.IsNull();
        }
        else if (operation == "make_sphere")
        {
            double r = doc.HasMember("radius") ? doc["radius"].GetDouble() : 50.0;
            BRepPrimAPI_MakeSphere maker(r);
            resultShape = maker.Shape();
            ok = !resultShape.IsNull();
        }
        else if (operation == "boolean_cut")
        {
            if (!doc.HasMember("target") || !doc.HasMember("tool"))
            {
                responseDoc.AddMember("status", "error", alloc);
                responseDoc.AddMember("message",
                    "boolean_cut requires 'target' and 'tool' ids", alloc);
                goto serialize;
            }
            std::string targetId = doc["target"].GetString();
            std::string toolId   = doc["tool"].GetString();
            auto itTarget = shapeMap.find(targetId);
            auto itTool   = shapeMap.find(toolId);
            if (itTarget == shapeMap.end() || itTool == shapeMap.end())
            {
                responseDoc.AddMember("status", "error", alloc);
                responseDoc.AddMember("message", "Unknown target or tool id", alloc);
                goto serialize;
            }
            BRepAlgoAPI_Cut cutter(itTarget->second, itTool->second);
            if (cutter.IsDone())
            {
                resultShape = cutter.Shape();
                ok = !resultShape.IsNull();
            }
            else
            {
                responseDoc.AddMember("status", "error", alloc);
                responseDoc.AddMember("message", "Boolean cut failed", alloc);
                goto serialize;
            }
        }
        else if (operation == "import_step")
        {
            if (!doc.HasMember("file"))
            {
                responseDoc.AddMember("status", "error", alloc);
                responseDoc.AddMember("message",
                    "import_step requires 'file' path", alloc);
                goto serialize;
            }
            std::string filename = doc["file"].GetString();
            STEPControl_Reader reader;
            IFSelect_ReturnStatus stat = reader.ReadFile(filename.c_str());
            if (stat != IFSelect_RetDone)
            {
                responseDoc.AddMember("status", "error", alloc);
                responseDoc.AddMember("message", "STEP file read failed", alloc);
                goto serialize;
            }
            reader.TransferRoots();
            resultShape = reader.OneShape();
            ok = !resultShape.IsNull();
        }
        else if (operation == "view")
        {
            if (!doc.HasMember("id"))
            {
                responseDoc.AddMember("status", "error", alloc);
                responseDoc.AddMember("message", "view requires 'id'", alloc);
                goto serialize;
            }
            std::string viewId = doc["id"].GetString();

            // Respond OK, then launch viewer
            StringBuffer tmpBuf;
            Writer<StringBuffer> tmpWriter(tmpBuf);
            Document tmpResp;
            tmpResp.SetObject();
            tmpResp.AddMember("status", "ok", alloc);
            tmpResp.AddMember("message", "Launching viewer...", alloc);
            tmpResp.Accept(tmpWriter);
            std::cout << tmpBuf.GetString() << std::endl;
            std::cout << "[Vison Engine] __END__\n" << std::flush;

            ShowShapeInViewer(viewId);
            return;   // already handled
        }
        else
        {
            responseDoc.AddMember("status", "error", alloc);
            std::string msg = "Unknown operation: " + operation;
            responseDoc.AddMember("message", Value(msg.c_str(), alloc), alloc);
            goto serialize;
        }

        // ---- Common success/failure handling ----
        if (ok)
        {
            shapeMap[id] = resultShape;
            responseDoc.AddMember("status", "ok", alloc);
            Value respId(id.c_str(), alloc);
            responseDoc.AddMember("id", respId, alloc);
            addShapeMetadata(responseDoc, resultShape, alloc);
        }
        else
        {
            responseDoc.AddMember("status", "error", alloc);
            responseDoc.AddMember("message",
                "Geometry creation returned null shape", alloc);
        }
    }

serialize:
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    responseDoc.Accept(writer);
    std::cout << buffer.GetString() << std::endl;
}

// ===== BLOCK 8: MAIN ENTRY POINT =====
int main(int argc, char** argv)
{
#ifdef USE_VIEWER
    // ========== DEBUG VIEWER MODE ==========
    std::cout << "=== Vison CAD Debug Viewer ===\n";
    BRepPrimAPI_MakeBox boxMaker(100.0, 100.0, 100.0);
    TopoDS_Shape box = boxMaker.Shape();
    if (box.IsNull()) return 1;
    g_shapeMap["_debug_box"] = box;
    ShowShapeInViewer("_debug_box");
    return 0;

#else
    // ========== HEADLESS ENGINE MODE ==========
    std::cout << "[Vison Engine] JSON mode. One compact JSON per line.\n";
    std::cout << "Ops: make_box, make_cylinder, make_sphere, boolean_cut, import_step, view\n";

    std::string line;
    while (std::getline(std::cin, line))
    {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos || line[start] != '{')
            continue;

        ProcessJsonInstruction(line, g_shapeMap);
        std::cout << "[Vison Engine] __END__\n" << std::flush;
    }

    std::cout << "[Vison Engine] Shutdown.\n";
    return 0;
#endif
}