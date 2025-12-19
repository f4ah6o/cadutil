/**
 * cadutil_core - Implementation
 *
 * Core library for cadutil (CAD Utility CLI).
 * Provides DXF/JWW file parsing, conversion, and validation.
 */

#include "librecad_core.h"
#include "drw_interface.h"
#include "libdxfrw.h"
#include "dl_jww.h"
#include "dl_creationinterface.h"
#include "jwwdoc.h"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <fstream>

/* Forward declaration for JWW export */
static LcError lc_document_save_jww(LcDocument* doc, const char* filename);

/* Thread-local error message */
static thread_local std::string g_last_error;

/* ============================================================================
 * Internal data structures
 * ============================================================================ */

struct LayerData {
    std::string name;
    int color = 7;
    std::string lineType = "CONTINUOUS";
    double lineWeight = 0.0;
    bool off = false;
    bool frozen = false;
    bool locked = false;
};

struct BlockData {
    std::string name;
    DRW_Coord basePoint{0.0, 0.0, 0.0};
    std::vector<DRW_Entity*> entities;
};

struct EntityData {
    LcEntityType type = LC_ENTITY_UNKNOWN;
    std::string layer;
    int color = 256; /* BYLAYER */
    std::string lineType = "BYLAYER";
    double lineWeight = -1.0; /* BYLAYER */
    int handle = 0;

    /* Geometry (simplified storage) */
    DRW_Coord point1{0,0,0};
    DRW_Coord point2{0,0,0};
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    std::string text;
    std::string blockName;
    double height = 0.0;
    double rotation = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    int vertexCount = 0;
    int degree = 0;
    bool closed = false;
};

/* ============================================================================
 * Document class (internal)
 * ============================================================================ */

class DocumentImpl : public DRW_Interface {
public:
    std::string filename;
    LcFormat format = LC_FORMAT_UNKNOWN;
    std::string dxfVersion;

    std::vector<LayerData> layers;
    std::vector<BlockData> blocks;
    std::vector<EntityData> entities;
    std::map<std::string, DRW_LType> lineTypes;
    std::map<std::string, DRW_Dimstyle> dimStyles;
    std::map<std::string, DRW_Textstyle> textStyles;

    DRW_Header header;
    DRW_Coord minBound{1e20, 1e20, 1e20};
    DRW_Coord maxBound{-1e20, -1e20, -1e20};

    /* Current block being filled (nullptr for modelspace) */
    BlockData* currentBlock = nullptr;

    /* Validation issues */
    std::vector<LcValidationIssue> validationIssues;

    /* Pointer to dxfRW for writing (set during save operation) */
    dxfRW* dxfWriter = nullptr;

    void updateBounds(const DRW_Coord& p) {
        minBound.x = std::min(minBound.x, p.x);
        minBound.y = std::min(minBound.y, p.y);
        minBound.z = std::min(minBound.z, p.z);
        maxBound.x = std::max(maxBound.x, p.x);
        maxBound.y = std::max(maxBound.y, p.y);
        maxBound.z = std::max(maxBound.z, p.z);
    }

    void addEntityData(const EntityData& e) {
        entities.push_back(e);
    }

    /* DRW_Interface implementation */
    void addHeader(const DRW_Header* data) override {
        if (data) {
            header = *data;
            /* Extract version */
            auto it = data->vars.find("$ACADVER");
            if (it != data->vars.end() && it->second->type() == DRW_Variant::STRING) {
                dxfVersion = *(it->second->content.s);
            }
        }
    }

    void addLType(const DRW_LType& data) override {
        lineTypes[data.name] = data;
    }

    void addLayer(const DRW_Layer& data) override {
        LayerData ld;
        ld.name = data.name;
        ld.color = data.color;
        ld.lineType = data.lineType;
        ld.lineWeight = data.lWeight;
        ld.off = (data.flags & 0x01) != 0;
        ld.frozen = (data.flags & 0x02) != 0;
        ld.locked = (data.flags & 0x04) != 0;
        layers.push_back(ld);
    }

    void addDimStyle(const DRW_Dimstyle& data) override {
        dimStyles[data.name] = data;
    }

    void addVport(const DRW_Vport& /*data*/) override {}
    void addView(const DRW_View& /*data*/) override {}
    void addUCS(const DRW_UCS& /*data*/) override {}

    void addTextStyle(const DRW_Textstyle& data) override {
        textStyles[data.name] = data;
    }

    void addAppId(const DRW_AppId& /*data*/) override {}

    void addBlock(const DRW_Block& data) override {
        BlockData bd;
        bd.name = data.name;
        bd.basePoint = data.basePoint;
        blocks.push_back(bd);
        currentBlock = &blocks.back();
    }

    void setBlock(const int /*handle*/) override {}

    void endBlock() override {
        currentBlock = nullptr;
    }

    void addPoint(const DRW_Point& data) override {
        EntityData e;
        e.type = LC_ENTITY_POINT;
        e.layer = data.layer;
        e.color = data.color;
        e.lineType = data.lineType;
        e.handle = data.handle;
        e.point1 = data.basePoint;
        addEntityData(e);
        updateBounds(data.basePoint);
    }

    void addLine(const DRW_Line& data) override {
        EntityData e;
        e.type = LC_ENTITY_LINE;
        e.layer = data.layer;
        e.color = data.color;
        e.lineType = data.lineType;
        e.handle = data.handle;
        e.point1 = data.basePoint;
        e.point2 = data.secPoint;
        addEntityData(e);
        updateBounds(data.basePoint);
        updateBounds(data.secPoint);
    }

    void addRay(const DRW_Ray& /*data*/) override {}
    void addXline(const DRW_Xline& /*data*/) override {}

    void addArc(const DRW_Arc& data) override {
        EntityData e;
        e.type = LC_ENTITY_ARC;
        e.layer = data.layer;
        e.color = data.color;
        e.lineType = data.lineType;
        e.handle = data.handle;
        e.point1 = data.basePoint;
        e.radius = data.radious;
        e.startAngle = data.staangle;
        e.endAngle = data.endangle;
        addEntityData(e);
        /* Approximate bounds */
        DRW_Coord c = data.basePoint;
        updateBounds({c.x - data.radious, c.y - data.radious, c.z});
        updateBounds({c.x + data.radious, c.y + data.radious, c.z});
    }

    void addCircle(const DRW_Circle& data) override {
        EntityData e;
        e.type = LC_ENTITY_CIRCLE;
        e.layer = data.layer;
        e.color = data.color;
        e.lineType = data.lineType;
        e.handle = data.handle;
        e.point1 = data.basePoint;
        e.radius = data.radious;
        addEntityData(e);
        DRW_Coord c = data.basePoint;
        updateBounds({c.x - data.radious, c.y - data.radious, c.z});
        updateBounds({c.x + data.radious, c.y + data.radious, c.z});
    }

    void addEllipse(const DRW_Ellipse& data) override {
        EntityData e;
        e.type = LC_ENTITY_ELLIPSE;
        e.layer = data.layer;
        e.color = data.color;
        e.lineType = data.lineType;
        e.handle = data.handle;
        e.point1 = data.basePoint;
        e.point2 = data.secPoint; /* Major axis endpoint */
        e.radius = data.ratio;    /* Ratio minor/major */
        addEntityData(e);
        /* Approximate bounds */
        double majorLen = std::sqrt(data.secPoint.x*data.secPoint.x +
                                    data.secPoint.y*data.secPoint.y);
        DRW_Coord c = data.basePoint;
        updateBounds({c.x - majorLen, c.y - majorLen, c.z});
        updateBounds({c.x + majorLen, c.y + majorLen, c.z});
    }

    void addLWPolyline(const DRW_LWPolyline& data) override {
        EntityData e;
        e.type = LC_ENTITY_LWPOLYLINE;
        e.layer = data.layer;
        e.color = data.color;
        e.lineType = data.lineType;
        e.handle = data.handle;
        e.vertexCount = static_cast<int>(data.vertlist.size());
        e.closed = (data.flags & 0x01) != 0;
        addEntityData(e);
        for (const auto& v : data.vertlist) {
            updateBounds({v->x, v->y, 0.0});
        }
    }

    void addPolyline(const DRW_Polyline& data) override {
        EntityData e;
        e.type = LC_ENTITY_POLYLINE;
        e.layer = data.layer;
        e.color = data.color;
        e.lineType = data.lineType;
        e.handle = data.handle;
        e.vertexCount = static_cast<int>(data.vertlist.size());
        e.closed = (data.flags & 0x01) != 0;
        addEntityData(e);
        for (const auto& v : data.vertlist) {
            updateBounds(v->basePoint);
        }
    }

    void addSpline(const DRW_Spline* data) override {
        if (!data) return;
        EntityData e;
        e.type = LC_ENTITY_SPLINE;
        e.layer = data->layer;
        e.color = data->color;
        e.lineType = data->lineType;
        e.handle = data->handle;
        e.vertexCount = static_cast<int>(data->controllist.size());
        e.degree = data->degree;
        e.closed = (data->flags & 0x01) != 0;
        addEntityData(e);
        for (const auto& cp : data->controllist) {
            updateBounds({cp->x, cp->y, cp->z});
        }
    }

    void addKnot(const DRW_Entity& /*data*/) override {}

    void addInsert(const DRW_Insert& data) override {
        EntityData e;
        e.type = LC_ENTITY_INSERT;
        e.layer = data.layer;
        e.color = data.color;
        e.lineType = data.lineType;
        e.handle = data.handle;
        e.blockName = data.name;
        e.point1 = data.basePoint;
        e.scaleX = data.xscale;
        e.scaleY = data.yscale;
        e.rotation = data.angle;
        addEntityData(e);
        updateBounds(data.basePoint);
    }

    void addTrace(const DRW_Trace& data) override {
        EntityData e;
        e.type = LC_ENTITY_TRACE;
        e.layer = data.layer;
        e.color = data.color;
        e.handle = data.handle;
        addEntityData(e);
    }

    void add3dFace(const DRW_3Dface& data) override {
        EntityData e;
        e.type = LC_ENTITY_3DFACE;
        e.layer = data.layer;
        e.color = data.color;
        e.handle = data.handle;
        addEntityData(e);
    }

    void addSolid(const DRW_Solid& data) override {
        EntityData e;
        e.type = LC_ENTITY_SOLID;
        e.layer = data.layer;
        e.color = data.color;
        e.handle = data.handle;
        addEntityData(e);
    }

    void addMText(const DRW_MText& data) override {
        EntityData e;
        e.type = LC_ENTITY_MTEXT;
        e.layer = data.layer;
        e.color = data.color;
        e.lineType = data.lineType;
        e.handle = data.handle;
        e.text = data.text;
        e.point1 = data.basePoint;
        e.height = data.height;
        addEntityData(e);
        updateBounds(data.basePoint);
    }

    void addText(const DRW_Text& data) override {
        EntityData e;
        e.type = LC_ENTITY_TEXT;
        e.layer = data.layer;
        e.color = data.color;
        e.lineType = data.lineType;
        e.handle = data.handle;
        e.text = data.text;
        e.point1 = data.basePoint;
        e.height = data.height;
        e.rotation = data.angle;
        addEntityData(e);
        updateBounds(data.basePoint);
    }

    void addTolerance(const DRW_Tolerance& /*tol*/) override {}

    void addDimAlign(const DRW_DimAligned* data) override {
        if (!data) return;
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        e.layer = data->layer;
        e.color = data->color;
        e.handle = data->handle;
        addEntityData(e);
    }

    void addDimLinear(const DRW_DimLinear* data) override {
        if (!data) return;
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        e.layer = data->layer;
        e.color = data->color;
        e.handle = data->handle;
        addEntityData(e);
    }

    void addDimRadial(const DRW_DimRadial* data) override {
        if (!data) return;
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        e.layer = data->layer;
        e.color = data->color;
        e.handle = data->handle;
        addEntityData(e);
    }

    void addDimDiametric(const DRW_DimDiametric* data) override {
        if (!data) return;
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        e.layer = data->layer;
        e.color = data->color;
        e.handle = data->handle;
        addEntityData(e);
    }

    void addDimAngular(const DRW_DimAngular* data) override {
        if (!data) return;
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        e.layer = data->layer;
        e.color = data->color;
        e.handle = data->handle;
        addEntityData(e);
    }

    void addDimAngular3P(const DRW_DimAngular3p* data) override {
        if (!data) return;
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        e.layer = data->layer;
        e.color = data->color;
        e.handle = data->handle;
        addEntityData(e);
    }

    void addDimOrdinate(const DRW_DimOrdinate* data) override {
        if (!data) return;
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        e.layer = data->layer;
        e.color = data->color;
        e.handle = data->handle;
        addEntityData(e);
    }

    void addLeader(const DRW_Leader* data) override {
        if (!data) return;
        EntityData e;
        e.type = LC_ENTITY_LEADER;
        e.layer = data->layer;
        e.color = data->color;
        e.handle = data->handle;
        addEntityData(e);
    }

    void addHatch(const DRW_Hatch* data) override {
        if (!data) return;
        EntityData e;
        e.type = LC_ENTITY_HATCH;
        e.layer = data->layer;
        e.color = data->color;
        e.handle = data->handle;
        addEntityData(e);
    }

    void addViewport(const DRW_Viewport& data) override {
        EntityData e;
        e.type = LC_ENTITY_VIEWPORT;
        e.layer = data.layer;
        e.handle = data.handle;
        addEntityData(e);
    }

    void addImage(const DRW_Image* data) override {
        if (!data) return;
        EntityData e;
        e.type = LC_ENTITY_IMAGE;
        e.layer = data->layer;
        e.color = data->color;
        e.handle = data->handle;
        addEntityData(e);
    }

    void linkImage(const DRW_ImageDef* /*data*/) override {}
    void addComment(const char* /*comment*/) override {}
    void addPlotSettings(const DRW_PlotSettings* /*data*/) override {}

    /* Write callbacks (for export) */
    void writeHeader(DRW_Header& data) override {
        data = header;
    }

    void writeBlocks() override {
        if (!dxfWriter) return;

        /* Write model space and paper space blocks first */
        DRW_Block modelSpace;
        modelSpace.name = "*Model_Space";
        modelSpace.flags = 0;
        dxfWriter->writeBlock(&modelSpace);

        DRW_Block paperSpace;
        paperSpace.name = "*Paper_Space";
        paperSpace.flags = 0;
        dxfWriter->writeBlock(&paperSpace);

        /* Write user-defined blocks */
        for (const auto& b : blocks) {
            /* Skip special blocks */
            if (b.name.empty() || b.name[0] == '*') continue;

            DRW_Block block;
            block.name = b.name;
            block.basePoint = b.basePoint;
            block.flags = 0;
            dxfWriter->writeBlock(&block);
        }
    }

    void writeBlockRecords() override {
        if (!dxfWriter) return;

        /* Write standard block records */
        dxfWriter->writeBlockRecord("*Model_Space");
        dxfWriter->writeBlockRecord("*Paper_Space");

        /* Write user-defined block records */
        for (const auto& b : blocks) {
            if (b.name.empty() || b.name[0] == '*') continue;
            dxfWriter->writeBlockRecord(b.name);
        }
    }

    void writeEntities() override {
        if (!dxfWriter) return;

        for (const auto& e : entities) {
            switch (e.type) {
                case LC_ENTITY_POINT: {
                    DRW_Point pt;
                    pt.layer = e.layer.empty() ? "0" : e.layer;
                    pt.color = e.color;
                    pt.lineType = e.lineType;
                    pt.basePoint = e.point1;
                    dxfWriter->writePoint(&pt);
                    break;
                }
                case LC_ENTITY_LINE: {
                    DRW_Line ln;
                    ln.layer = e.layer.empty() ? "0" : e.layer;
                    ln.color = e.color;
                    ln.lineType = e.lineType;
                    ln.basePoint = e.point1;
                    ln.secPoint = e.point2;
                    dxfWriter->writeLine(&ln);
                    break;
                }
                case LC_ENTITY_CIRCLE: {
                    DRW_Circle cir;
                    cir.layer = e.layer.empty() ? "0" : e.layer;
                    cir.color = e.color;
                    cir.lineType = e.lineType;
                    cir.basePoint = e.point1;
                    cir.radious = e.radius;
                    dxfWriter->writeCircle(&cir);
                    break;
                }
                case LC_ENTITY_ARC: {
                    DRW_Arc arc;
                    arc.layer = e.layer.empty() ? "0" : e.layer;
                    arc.color = e.color;
                    arc.lineType = e.lineType;
                    arc.basePoint = e.point1;
                    arc.radious = e.radius;
                    arc.staangle = e.startAngle;
                    arc.endangle = e.endAngle;
                    dxfWriter->writeArc(&arc);
                    break;
                }
                case LC_ENTITY_ELLIPSE: {
                    DRW_Ellipse ell;
                    ell.layer = e.layer.empty() ? "0" : e.layer;
                    ell.color = e.color;
                    ell.lineType = e.lineType;
                    ell.basePoint = e.point1;
                    ell.secPoint = e.point2;
                    ell.ratio = e.radius;
                    ell.staparam = e.startAngle;
                    ell.endparam = e.endAngle;
                    dxfWriter->writeEllipse(&ell);
                    break;
                }
                case LC_ENTITY_TEXT: {
                    DRW_Text txt;
                    txt.layer = e.layer.empty() ? "0" : e.layer;
                    txt.color = e.color;
                    txt.lineType = e.lineType;
                    txt.basePoint = e.point1;
                    txt.secPoint = e.point1;
                    txt.text = e.text;
                    txt.height = e.height > 0 ? e.height : 2.5;
                    txt.angle = e.rotation;
                    txt.widthscale = 1.0;
                    txt.oblique = 0.0;
                    txt.style = "STANDARD";
                    txt.textgen = 0;
                    txt.alignH = DRW_Text::HLeft;
                    txt.alignV = DRW_Text::VBaseLine;
                    dxfWriter->writeText(&txt);
                    break;
                }
                case LC_ENTITY_MTEXT: {
                    DRW_MText mtxt;
                    mtxt.layer = e.layer.empty() ? "0" : e.layer;
                    mtxt.color = e.color;
                    mtxt.lineType = e.lineType;
                    mtxt.basePoint = e.point1;
                    mtxt.text = e.text;
                    mtxt.height = e.height > 0 ? e.height : 2.5;
                    mtxt.widthscale = 100.0;
                    mtxt.textgen = 1;
                    mtxt.alignH = DRW_MText::HCenter;
                    mtxt.alignV = DRW_MText::VBottom;
                    mtxt.style = "STANDARD";
                    mtxt.angle = e.rotation;
                    mtxt.interlin = 1.0;
                    dxfWriter->writeMText(&mtxt);
                    break;
                }
                case LC_ENTITY_INSERT: {
                    DRW_Insert ins;
                    ins.layer = e.layer.empty() ? "0" : e.layer;
                    ins.color = e.color;
                    ins.lineType = e.lineType;
                    ins.name = e.blockName;
                    ins.basePoint = e.point1;
                    ins.xscale = e.scaleX;
                    ins.yscale = e.scaleY;
                    ins.zscale = 1.0;
                    ins.angle = e.rotation;
                    ins.colcount = 1;
                    ins.rowcount = 1;
                    ins.colspace = 0.0;
                    ins.rowspace = 0.0;
                    dxfWriter->writeInsert(&ins);
                    break;
                }
                case LC_ENTITY_SOLID: {
                    DRW_Solid sol;
                    sol.layer = e.layer.empty() ? "0" : e.layer;
                    sol.color = e.color;
                    sol.basePoint = e.point1;
                    sol.secPoint = e.point1;
                    sol.thirdPoint = e.point1;
                    sol.fourPoint = e.point1;
                    dxfWriter->writeSolid(&sol);
                    break;
                }
                case LC_ENTITY_TRACE: {
                    DRW_Trace tr;
                    tr.layer = e.layer.empty() ? "0" : e.layer;
                    tr.color = e.color;
                    tr.basePoint = e.point1;
                    tr.secPoint = e.point1;
                    tr.thirdPoint = e.point1;
                    tr.fourPoint = e.point1;
                    dxfWriter->writeTrace(&tr);
                    break;
                }
                case LC_ENTITY_3DFACE: {
                    DRW_3Dface face;
                    face.layer = e.layer.empty() ? "0" : e.layer;
                    face.color = e.color;
                    face.basePoint = e.point1;
                    face.secPoint = e.point1;
                    face.thirdPoint = e.point1;
                    face.fourPoint = e.point1;
                    face.invisibleflag = 0;
                    dxfWriter->write3dface(&face);
                    break;
                }
                default:
                    /* Skip unsupported entity types for now */
                    break;
            }
        }
    }

    void writeLTypes() override {
        if (!dxfWriter) return;

        /* Write standard line types - these are handled by libdxfrw */
        for (const auto& lt : lineTypes) {
            DRW_LType ltype = lt.second;
            dxfWriter->writeLineType(&ltype);
        }
    }

    void writeLayers() override {
        if (!dxfWriter) return;

        /* Always write layer 0 */
        bool hasLayer0 = false;
        for (const auto& l : layers) {
            if (l.name == "0") {
                hasLayer0 = true;
                break;
            }
        }

        if (!hasLayer0) {
            DRW_Layer layer0;
            layer0.name = "0";
            layer0.color = 7;
            layer0.lineType = "CONTINUOUS";
            layer0.flags = 0;
            layer0.plotF = true;
            layer0.lWeight = DRW_LW_Conv::widthDefault;
            dxfWriter->writeLayer(&layer0);
        }

        /* Write all layers */
        for (const auto& l : layers) {
            DRW_Layer layer;
            layer.name = l.name;
            layer.color = l.color;
            layer.lineType = l.lineType;
            layer.flags = 0;
            if (l.off) layer.flags |= 0x01;
            if (l.frozen) layer.flags |= 0x02;
            if (l.locked) layer.flags |= 0x04;
            layer.plotF = true;
            layer.lWeight = DRW_LW_Conv::widthDefault;
            dxfWriter->writeLayer(&layer);
        }
    }

    void writeTextstyles() override {
        if (!dxfWriter) return;

        /* Write STANDARD text style if no styles defined */
        bool hasStandard = false;
        for (const auto& ts : textStyles) {
            if (ts.first == "STANDARD" || ts.first == "Standard") {
                hasStandard = true;
                break;
            }
        }

        if (!hasStandard) {
            DRW_Textstyle style;
            style.name = "STANDARD";
            style.height = 0.0;
            style.width = 1.0;
            style.oblique = 0.0;
            style.genFlag = 0;
            style.lastHeight = 2.5;
            style.font = "txt";
            style.flags = 0;
            dxfWriter->writeTextstyle(&style);
        }

        /* Write all text styles */
        for (const auto& ts : textStyles) {
            DRW_Textstyle style = ts.second;
            dxfWriter->writeTextstyle(&style);
        }
    }

    void writeDimstyles() override {
        if (!dxfWriter) return;

        /* Write STANDARD dim style if no styles defined */
        bool hasStandard = false;
        for (const auto& ds : dimStyles) {
            if (ds.first == "STANDARD" || ds.first == "Standard") {
                hasStandard = true;
                break;
            }
        }

        if (!hasStandard) {
            DRW_Dimstyle style;
            style.name = "STANDARD";
            style.flags = 0;
            style.dimasz = 2.5;
            style.dimexo = 0.625;
            style.dimdli = 3.75;
            style.dimexe = 1.25;
            style.dimtxt = 2.5;
            style.dimtsz = 0.0;
            style.dimcen = 2.5;
            style.dimgap = 0.625;
            dxfWriter->writeDimstyle(&style);
        }

        /* Write all dim styles */
        for (const auto& ds : dimStyles) {
            DRW_Dimstyle style = ds.second;
            dxfWriter->writeDimstyle(&style);
        }
    }

    void writeVports() override {
        if (!dxfWriter) return;

        /* Write default viewport */
        DRW_Vport vport;
        vport.name = "*ACTIVE";
        vport.flags = 0;
        vport.lowerLeft = {0.0, 0.0, 0.0};
        vport.UpperRight = {1.0, 1.0, 0.0};
        vport.center = {0.0, 0.0, 0.0};
        vport.snapBase = {0.0, 0.0, 0.0};
        vport.snapSpacing = {10.0, 10.0, 0.0};
        vport.gridSpacing = {10.0, 10.0, 0.0};
        vport.viewDir = {0.0, 0.0, 1.0};
        vport.viewTarget = {0.0, 0.0, 0.0};
        vport.height = 100.0;
        vport.ratio = 1.0;
        vport.lensHeight = 50.0;
        vport.frontClip = 0.0;
        vport.backClip = 0.0;
        vport.snapAngle = 0.0;
        vport.twistAngle = 0.0;
        dxfWriter->writeVport(&vport);
    }

    void writeViews() override {
        /* No views to write by default */
    }

    void writeUCSs() override {
        /* No UCS to write by default */
    }

    void writeAppId() override {
        if (!dxfWriter) return;

        DRW_AppId appId;
        appId.name = "ACAD";
        appId.flags = 0;
        dxfWriter->writeAppId(&appId);
    }

    void writeObjects() override {
        /* Objects section - usually empty for basic DXF */
    }
};

/* ============================================================================
 * JWW Reader implementation
 * ============================================================================ */

class JwwReaderImpl : public DL_CreationInterface {
public:
    DocumentImpl* doc = nullptr;

    void addLayer(const DL_LayerData& data) override {
        LayerData ld;
        ld.name = data.name;
        doc->layers.push_back(ld);
    }

    void addBlock(const DL_BlockData& data) override {
        BlockData bd;
        bd.name = data.name;
        bd.basePoint = {data.bpx, data.bpy, data.bpz};
        doc->blocks.push_back(bd);
    }

    void endBlock() override {}

    void addPoint(const DL_PointData& data) override {
        EntityData e;
        e.type = LC_ENTITY_POINT;
        e.point1 = {data.x, data.y, data.z};
        doc->addEntityData(e);
        doc->updateBounds(e.point1);
    }

    void addLine(const DL_LineData& data) override {
        EntityData e;
        e.type = LC_ENTITY_LINE;
        e.point1 = {data.x1, data.y1, data.z1};
        e.point2 = {data.x2, data.y2, data.z2};
        doc->addEntityData(e);
        doc->updateBounds(e.point1);
        doc->updateBounds(e.point2);
    }

    void addArc(const DL_ArcData& data) override {
        EntityData e;
        e.type = LC_ENTITY_ARC;
        e.point1 = {data.cx, data.cy, data.cz};
        e.radius = data.radius;
        e.startAngle = data.angle1;
        e.endAngle = data.angle2;
        doc->addEntityData(e);
    }

    void addCircle(const DL_CircleData& data) override {
        EntityData e;
        e.type = LC_ENTITY_CIRCLE;
        e.point1 = {data.cx, data.cy, data.cz};
        e.radius = data.radius;
        doc->addEntityData(e);
    }

    void addEllipse(const DL_EllipseData& data) override {
        EntityData e;
        e.type = LC_ENTITY_ELLIPSE;
        e.point1 = {data.cx, data.cy, data.cz};
        e.point2 = {data.mx, data.my, data.mz};
        e.radius = data.ratio;
        doc->addEntityData(e);
    }

    void addPolyline(const DL_PolylineData& data) override {
        EntityData e;
        e.type = LC_ENTITY_POLYLINE;
        e.vertexCount = static_cast<int>(data.number);
        e.closed = (data.flags & 0x01) != 0;
        doc->addEntityData(e);
    }

    void addVertex(const DL_VertexData& data) override {
        doc->updateBounds({data.x, data.y, data.z});
    }

    void addSpline(const DL_SplineData& data) override {
        EntityData e;
        e.type = LC_ENTITY_SPLINE;
        e.degree = data.degree;
        e.closed = (data.flags & 0x01) != 0;
        doc->addEntityData(e);
    }

    void addControlPoint(const DL_ControlPointData& data) override {
        doc->updateBounds({data.x, data.y, data.z});
    }

    void addKnot(const DL_KnotData& /*data*/) override {}

    void addInsert(const DL_InsertData& data) override {
        EntityData e;
        e.type = LC_ENTITY_INSERT;
        e.blockName = data.name;
        e.point1 = {data.ipx, data.ipy, data.ipz};
        e.scaleX = data.sx;
        e.scaleY = data.sy;
        e.rotation = data.angle;
        doc->addEntityData(e);
    }

    void addMText(const DL_MTextData& data) override {
        EntityData e;
        e.type = LC_ENTITY_MTEXT;
        e.text = data.text;
        e.point1 = {data.ipx, data.ipy, data.ipz};
        e.height = data.height;
        doc->addEntityData(e);
    }

    void addText(const DL_TextData& data) override {
        EntityData e;
        e.type = LC_ENTITY_TEXT;
        e.text = data.text;
        e.point1 = {data.ipx, data.ipy, data.ipz};
        e.height = data.height;
        e.rotation = data.angle;
        doc->addEntityData(e);
    }

    void addDimAlign(const DL_DimensionData& /*data*/, const DL_DimAlignedData& /*edata*/) override {
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        doc->addEntityData(e);
    }

    void addDimLinear(const DL_DimensionData& /*data*/, const DL_DimLinearData& /*edata*/) override {
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        doc->addEntityData(e);
    }

    void addDimRadial(const DL_DimensionData& /*data*/, const DL_DimRadialData& /*edata*/) override {
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        doc->addEntityData(e);
    }

    void addDimDiametric(const DL_DimensionData& /*data*/, const DL_DimDiametricData& /*edata*/) override {
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        doc->addEntityData(e);
    }

    void addDimAngular(const DL_DimensionData& /*data*/, const DL_DimAngularData& /*edata*/) override {
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        doc->addEntityData(e);
    }

    void addDimAngular3P(const DL_DimensionData& /*data*/, const DL_DimAngular3PData& /*edata*/) override {
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        doc->addEntityData(e);
    }

    void addDimOrdinate(const DL_DimensionData& /*data*/, const DL_DimOrdinateData& /*edata*/) override {
        EntityData e;
        e.type = LC_ENTITY_DIMENSION;
        doc->addEntityData(e);
    }

    void addLeader(const DL_LeaderData& /*data*/) override {
        EntityData e;
        e.type = LC_ENTITY_LEADER;
        doc->addEntityData(e);
    }

    void addLeaderVertex(const DL_LeaderVertexData& /*data*/) override {}

    void addHatch(const DL_HatchData& /*data*/) override {
        EntityData e;
        e.type = LC_ENTITY_HATCH;
        doc->addEntityData(e);
    }

    void addHatchLoop(const DL_HatchLoopData& /*data*/) override {}
    void addHatchEdge(const DL_HatchEdgeData& /*data*/) override {}

    void addImage(const DL_ImageData& /*data*/) override {
        EntityData e;
        e.type = LC_ENTITY_IMAGE;
        doc->addEntityData(e);
    }

    void linkImage(const DL_ImageDefData& /*data*/) override {}

    void addTrace(const DL_TraceData& /*data*/) override {
        EntityData e;
        e.type = LC_ENTITY_TRACE;
        doc->addEntityData(e);
    }

    void addSolid(const DL_SolidData& /*data*/) override {
        EntityData e;
        e.type = LC_ENTITY_SOLID;
        doc->addEntityData(e);
    }

    void add3dFace(const DL_3dFaceData& /*data*/) override {
        EntityData e;
        e.type = LC_ENTITY_3DFACE;
        doc->addEntityData(e);
    }

    void endSequence() override {}
    void endEntity() override {}
    void addComment(const char* /*comment*/) override {}
    void addMTextChunk(const char* /*text*/) override {}
    void setVariableVector(const char* /*key*/, double /*v1*/, double /*v2*/, double /*v3*/, int /*code*/) override {}
    void setVariableString(const char* /*key*/, const char* /*value*/, int /*code*/) override {}
    void setVariableInt(const char* /*key*/, int /*value*/, int /*code*/) override {}
    void setVariableDouble(const char* /*key*/, double /*value*/, int /*code*/) override {}
};

/* ============================================================================
 * Helper functions
 * ============================================================================ */

static char* strdup_cpp(const std::string& s) {
    char* result = static_cast<char*>(malloc(s.size() + 1));
    if (result) {
        std::memcpy(result, s.c_str(), s.size() + 1);
    }
    return result;
}

static std::string escapeJson(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

static DRW::Version lcVersionToDrw(LcDxfVersion v) {
    switch (v) {
        case LC_DXF_VERSION_R12: return DRW::AC1009;
        case LC_DXF_VERSION_R14: return DRW::AC1014;
        case LC_DXF_VERSION_2000: return DRW::AC1015;
        case LC_DXF_VERSION_2004: return DRW::AC1018;
        case LC_DXF_VERSION_2007: return DRW::AC1021;
        case LC_DXF_VERSION_2010: return DRW::AC1024;
        case LC_DXF_VERSION_2013: return DRW::AC1027;
        case LC_DXF_VERSION_2018: return DRW::AC1032;
        default: return DRW::AC1021;
    }
}

static const char* entityTypeName(LcEntityType t) {
    switch (t) {
        case LC_ENTITY_POINT: return "POINT";
        case LC_ENTITY_LINE: return "LINE";
        case LC_ENTITY_CIRCLE: return "CIRCLE";
        case LC_ENTITY_ARC: return "ARC";
        case LC_ENTITY_ELLIPSE: return "ELLIPSE";
        case LC_ENTITY_POLYLINE: return "POLYLINE";
        case LC_ENTITY_LWPOLYLINE: return "LWPOLYLINE";
        case LC_ENTITY_SPLINE: return "SPLINE";
        case LC_ENTITY_TEXT: return "TEXT";
        case LC_ENTITY_MTEXT: return "MTEXT";
        case LC_ENTITY_INSERT: return "INSERT";
        case LC_ENTITY_HATCH: return "HATCH";
        case LC_ENTITY_DIMENSION: return "DIMENSION";
        case LC_ENTITY_LEADER: return "LEADER";
        case LC_ENTITY_SOLID: return "SOLID";
        case LC_ENTITY_TRACE: return "TRACE";
        case LC_ENTITY_3DFACE: return "3DFACE";
        case LC_ENTITY_IMAGE: return "IMAGE";
        case LC_ENTITY_VIEWPORT: return "VIEWPORT";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * C API Implementation
 * ============================================================================ */

extern "C" {

const char* lc_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             LIBRECAD_CORE_VERSION_MAJOR,
             LIBRECAD_CORE_VERSION_MINOR,
             LIBRECAD_CORE_VERSION_PATCH);
    return version;
}

const char* lc_last_error(void) {
    return g_last_error.c_str();
}

LcFormat lc_detect_format(const char* filename) {
    if (!filename) return LC_FORMAT_UNKNOWN;

    std::string fn(filename);
    size_t dotPos = fn.rfind('.');
    if (dotPos == std::string::npos) return LC_FORMAT_UNKNOWN;

    std::string ext = fn.substr(dotPos + 1);
    /* Convert to lowercase */
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ext == "dxf") return LC_FORMAT_DXF;
    if (ext == "dwg") return LC_FORMAT_DWG;
    if (ext == "jww") return LC_FORMAT_JWW;
    if (ext == "jwc") return LC_FORMAT_JWC;

    return LC_FORMAT_UNKNOWN;
}

LcDocument* lc_document_open(const char* filename) {
    if (!filename) {
        g_last_error = "Filename is null";
        return nullptr;
    }

    /* Check file exists */
    std::ifstream f(filename);
    if (!f.good()) {
        g_last_error = "File not found: " + std::string(filename);
        return nullptr;
    }
    f.close();

    LcFormat format = lc_detect_format(filename);

    auto doc = std::make_unique<DocumentImpl>();
    doc->filename = filename;
    doc->format = format;

    bool success = false;

    if (format == LC_FORMAT_DXF || format == LC_FORMAT_DWG) {
        dxfRW dxf(filename);
        success = dxf.read(doc.get(), false);
        if (!success) {
            g_last_error = "Failed to read DXF file";
        }
    } else if (format == LC_FORMAT_JWW || format == LC_FORMAT_JWC) {
        DL_Jww jww;
        JwwReaderImpl reader;
        reader.doc = doc.get();
        success = jww.in(filename, &reader);
        if (!success) {
            g_last_error = "Failed to read JWW file";
        }
    } else {
        g_last_error = "Unsupported file format";
        return nullptr;
    }

    if (!success) {
        return nullptr;
    }

    return reinterpret_cast<LcDocument*>(doc.release());
}

LcError lc_document_save(LcDocument* doc, const char* filename, LcDxfVersion version) {
    if (!doc || !filename) {
        g_last_error = "Invalid arguments";
        return LC_ERR_INVALID_ARGUMENT;
    }

    auto* impl = reinterpret_cast<DocumentImpl*>(doc);
    LcFormat outFormat = lc_detect_format(filename);

    if (outFormat == LC_FORMAT_DXF) {
        dxfRW dxf(filename);
        /* Set the dxfWriter pointer so writeEntities() etc. can use it */
        impl->dxfWriter = &dxf;
        bool success = dxf.write(impl, lcVersionToDrw(version), false);
        impl->dxfWriter = nullptr;
        if (!success) {
            g_last_error = "Failed to write DXF file";
            return LC_ERR_WRITE_ERROR;
        }
    } else if (outFormat == LC_FORMAT_JWW) {
        /* JWW export - see lc_document_save_jww() */
        return lc_document_save_jww(doc, filename);
    } else {
        g_last_error = "Unsupported output format";
        return LC_ERR_INVALID_FORMAT;
    }

    return LC_OK;
}

void lc_document_close(LcDocument* doc) {
    if (doc) {
        delete reinterpret_cast<DocumentImpl*>(doc);
    }
}

LcError lc_convert(const char* input_file, const char* output_file, LcDxfVersion dxf_version) {
    LcDocument* doc = lc_document_open(input_file);
    if (!doc) {
        return LC_ERR_READ_ERROR;
    }

    LcError err = lc_document_save(doc, output_file, dxf_version);
    lc_document_close(doc);
    return err;
}

LcFileInfo* lc_get_file_info(const char* filename, LcDetailLevel detail) {
    LcDocument* doc = lc_document_open(filename);
    if (!doc) {
        return nullptr;
    }

    LcFileInfo* info = lc_document_get_info(doc, detail);
    lc_document_close(doc);
    return info;
}

LcFileInfo* lc_document_get_info(LcDocument* doc, LcDetailLevel detail) {
    if (!doc) return nullptr;

    auto* impl = reinterpret_cast<DocumentImpl*>(doc);
    auto* info = static_cast<LcFileInfo*>(calloc(1, sizeof(LcFileInfo)));
    if (!info) return nullptr;

    /* Basic info */
    info->filename = strdup_cpp(impl->filename);
    info->format = impl->format;
    info->dxf_version = strdup_cpp(impl->dxfVersion);
    info->layer_count = static_cast<int>(impl->layers.size());
    info->block_count = static_cast<int>(impl->blocks.size());
    info->entity_count = static_cast<int>(impl->entities.size());

    /* Bounds */
    info->bounds.min = {impl->minBound.x, impl->minBound.y, impl->minBound.z};
    info->bounds.max = {impl->maxBound.x, impl->maxBound.y, impl->maxBound.z};

    /* Entity type counts */
    std::memset(info->entity_counts, 0, sizeof(info->entity_counts));
    for (const auto& e : impl->entities) {
        if (e.type < 20) {
            info->entity_counts[e.type]++;
        }
    }

    /* Layer details */
    if (detail >= LC_DETAIL_NORMAL && !impl->layers.empty()) {
        info->layers_len = static_cast<int>(impl->layers.size());
        info->layers = static_cast<LcLayerInfo*>(calloc(info->layers_len, sizeof(LcLayerInfo)));
        for (int i = 0; i < info->layers_len; i++) {
            const auto& l = impl->layers[i];
            info->layers[i].name = strdup_cpp(l.name);
            info->layers[i].color = l.color;
            info->layers[i].line_type = strdup_cpp(l.lineType);
            info->layers[i].line_weight = l.lineWeight;
            info->layers[i].is_off = l.off ? 1 : 0;
            info->layers[i].is_frozen = l.frozen ? 1 : 0;
            info->layers[i].is_locked = l.locked ? 1 : 0;
        }
    }

    /* Block details */
    if (detail >= LC_DETAIL_NORMAL && !impl->blocks.empty()) {
        info->blocks_len = static_cast<int>(impl->blocks.size());
        info->blocks = static_cast<LcBlockInfo*>(calloc(info->blocks_len, sizeof(LcBlockInfo)));
        for (int i = 0; i < info->blocks_len; i++) {
            const auto& b = impl->blocks[i];
            info->blocks[i].name = strdup_cpp(b.name);
            info->blocks[i].base_point = {b.basePoint.x, b.basePoint.y, b.basePoint.z};
            info->blocks[i].entity_count = static_cast<int>(b.entities.size());
        }
    }

    /* Entity details */
    if (detail >= LC_DETAIL_VERBOSE && !impl->entities.empty()) {
        info->entities_len = static_cast<int>(impl->entities.size());
        info->entities = static_cast<LcEntityInfo*>(calloc(info->entities_len, sizeof(LcEntityInfo)));
        for (int i = 0; i < info->entities_len; i++) {
            const auto& e = impl->entities[i];
            info->entities[i].type = e.type;
            info->entities[i].layer = strdup_cpp(e.layer);
            info->entities[i].color = e.color;
            info->entities[i].line_type = strdup_cpp(e.lineType);
            info->entities[i].line_weight = e.lineWeight;
            info->entities[i].handle = e.handle;

            if (detail >= LC_DETAIL_FULL) {
                /* Fill geometry data based on type */
                switch (e.type) {
                    case LC_ENTITY_POINT:
                        info->entities[i].data.point.point = {e.point1.x, e.point1.y, e.point1.z};
                        break;
                    case LC_ENTITY_LINE:
                        info->entities[i].data.line.start = {e.point1.x, e.point1.y, e.point1.z};
                        info->entities[i].data.line.end = {e.point2.x, e.point2.y, e.point2.z};
                        break;
                    case LC_ENTITY_CIRCLE:
                        info->entities[i].data.circle.center = {e.point1.x, e.point1.y, e.point1.z};
                        info->entities[i].data.circle.radius = e.radius;
                        break;
                    case LC_ENTITY_ARC:
                        info->entities[i].data.arc.center = {e.point1.x, e.point1.y, e.point1.z};
                        info->entities[i].data.arc.radius = e.radius;
                        info->entities[i].data.arc.start_angle = e.startAngle;
                        info->entities[i].data.arc.end_angle = e.endAngle;
                        break;
                    case LC_ENTITY_TEXT:
                    case LC_ENTITY_MTEXT:
                        info->entities[i].data.text.text = strdup_cpp(e.text);
                        info->entities[i].data.text.position = {e.point1.x, e.point1.y, e.point1.z};
                        info->entities[i].data.text.height = e.height;
                        info->entities[i].data.text.rotation = e.rotation;
                        break;
                    case LC_ENTITY_INSERT:
                        info->entities[i].data.insert.block_name = strdup_cpp(e.blockName);
                        info->entities[i].data.insert.position = {e.point1.x, e.point1.y, e.point1.z};
                        info->entities[i].data.insert.scale_x = e.scaleX;
                        info->entities[i].data.insert.scale_y = e.scaleY;
                        info->entities[i].data.insert.rotation = e.rotation;
                        break;
                    case LC_ENTITY_POLYLINE:
                    case LC_ENTITY_LWPOLYLINE:
                        info->entities[i].data.polyline.vertex_count = e.vertexCount;
                        info->entities[i].data.polyline.is_closed = e.closed ? 1 : 0;
                        break;
                    case LC_ENTITY_SPLINE:
                        info->entities[i].data.spline.control_point_count = e.vertexCount;
                        info->entities[i].data.spline.degree = e.degree;
                        info->entities[i].data.spline.is_closed = e.closed ? 1 : 0;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    return info;
}

void lc_file_info_free(LcFileInfo* info) {
    if (!info) return;

    free(info->filename);
    free(info->dxf_version);

    if (info->layers) {
        for (int i = 0; i < info->layers_len; i++) {
            free(info->layers[i].name);
            free(info->layers[i].line_type);
        }
        free(info->layers);
    }

    if (info->blocks) {
        for (int i = 0; i < info->blocks_len; i++) {
            free(info->blocks[i].name);
        }
        free(info->blocks);
    }

    if (info->entities) {
        for (int i = 0; i < info->entities_len; i++) {
            free(info->entities[i].layer);
            free(info->entities[i].line_type);
            /* Free type-specific strings */
            switch (info->entities[i].type) {
                case LC_ENTITY_TEXT:
                case LC_ENTITY_MTEXT:
                    free(info->entities[i].data.text.text);
                    break;
                case LC_ENTITY_INSERT:
                    free(info->entities[i].data.insert.block_name);
                    break;
                default:
                    break;
            }
        }
        free(info->entities);
    }

    free(info);
}

char* lc_file_info_to_json(const LcFileInfo* info) {
    if (!info) return nullptr;

    std::ostringstream json;
    json << "{\n";
    json << "  \"filename\": \"" << escapeJson(info->filename ? info->filename : "") << "\",\n";
    json << "  \"format\": " << static_cast<int>(info->format) << ",\n";
    json << "  \"dxf_version\": \"" << escapeJson(info->dxf_version ? info->dxf_version : "") << "\",\n";
    json << "  \"layer_count\": " << info->layer_count << ",\n";
    json << "  \"block_count\": " << info->block_count << ",\n";
    json << "  \"entity_count\": " << info->entity_count << ",\n";

    /* Bounds */
    json << "  \"bounds\": {\n";
    json << "    \"min\": [" << info->bounds.min.x << ", " << info->bounds.min.y << ", " << info->bounds.min.z << "],\n";
    json << "    \"max\": [" << info->bounds.max.x << ", " << info->bounds.max.y << ", " << info->bounds.max.z << "]\n";
    json << "  },\n";

    /* Entity counts by type */
    json << "  \"entity_counts\": {\n";
    bool first = true;
    for (int i = 1; i < 20; i++) {
        if (info->entity_counts[i] > 0) {
            if (!first) json << ",\n";
            json << "    \"" << entityTypeName(static_cast<LcEntityType>(i)) << "\": " << info->entity_counts[i];
            first = false;
        }
    }
    json << "\n  }";

    /* Layers */
    if (info->layers && info->layers_len > 0) {
        json << ",\n  \"layers\": [\n";
        for (int i = 0; i < info->layers_len; i++) {
            json << "    {";
            json << "\"name\": \"" << escapeJson(info->layers[i].name ? info->layers[i].name : "") << "\", ";
            json << "\"color\": " << info->layers[i].color << ", ";
            json << "\"line_type\": \"" << escapeJson(info->layers[i].line_type ? info->layers[i].line_type : "") << "\", ";
            json << "\"frozen\": " << (info->layers[i].is_frozen ? "true" : "false") << ", ";
            json << "\"locked\": " << (info->layers[i].is_locked ? "true" : "false");
            json << "}" << (i < info->layers_len - 1 ? "," : "") << "\n";
        }
        json << "  ]";
    }

    /* Blocks */
    if (info->blocks && info->blocks_len > 0) {
        json << ",\n  \"blocks\": [\n";
        for (int i = 0; i < info->blocks_len; i++) {
            json << "    {";
            json << "\"name\": \"" << escapeJson(info->blocks[i].name ? info->blocks[i].name : "") << "\", ";
            json << "\"base_point\": [" << info->blocks[i].base_point.x << ", "
                 << info->blocks[i].base_point.y << ", " << info->blocks[i].base_point.z << "], ";
            json << "\"entity_count\": " << info->blocks[i].entity_count;
            json << "}" << (i < info->blocks_len - 1 ? "," : "") << "\n";
        }
        json << "  ]";
    }

    /* Entities (abbreviated in verbose mode) */
    if (info->entities && info->entities_len > 0) {
        json << ",\n  \"entities\": [\n";
        for (int i = 0; i < info->entities_len; i++) {
            const auto& e = info->entities[i];
            json << "    {";
            json << "\"type\": \"" << entityTypeName(e.type) << "\", ";
            json << "\"layer\": \"" << escapeJson(e.layer ? e.layer : "") << "\", ";
            json << "\"color\": " << e.color << ", ";
            json << "\"handle\": " << e.handle;
            json << "}" << (i < info->entities_len - 1 ? "," : "") << "\n";
        }
        json << "  ]";
    }

    json << "\n}\n";

    return strdup_cpp(json.str());
}

LcValidationResult* lc_validate(const char* filename) {
    LcDocument* doc = lc_document_open(filename);
    if (!doc) {
        /* Return result with error */
        auto* result = static_cast<LcValidationResult*>(calloc(1, sizeof(LcValidationResult)));
        result->is_valid = 0;
        result->issue_count = 1;
        result->issues = static_cast<LcValidationIssue*>(calloc(1, sizeof(LcValidationIssue)));
        result->issues[0].severity = LC_SEVERITY_ERROR;
        result->issues[0].code = strdup_cpp("FILE_ERROR");
        result->issues[0].message = strdup_cpp(g_last_error);
        result->issues[0].location = strdup_cpp(filename);
        return result;
    }

    LcValidationResult* result = lc_document_validate(doc);
    lc_document_close(doc);
    return result;
}

LcValidationResult* lc_document_validate(LcDocument* doc) {
    if (!doc) return nullptr;

    auto* impl = reinterpret_cast<DocumentImpl*>(doc);
    auto* result = static_cast<LcValidationResult*>(calloc(1, sizeof(LcValidationResult)));

    std::vector<LcValidationIssue> issues;

    /* Check for empty drawing */
    if (impl->entities.empty()) {
        LcValidationIssue issue;
        issue.severity = LC_SEVERITY_WARNING;
        issue.code = strdup_cpp("EMPTY_DRAWING");
        issue.message = strdup_cpp("Drawing contains no entities");
        issue.location = strdup_cpp("");
        issues.push_back(issue);
    }

    /* Check for missing layer 0 */
    bool hasLayer0 = false;
    for (const auto& l : impl->layers) {
        if (l.name == "0") {
            hasLayer0 = true;
            break;
        }
    }
    if (!hasLayer0 && !impl->layers.empty()) {
        LcValidationIssue issue;
        issue.severity = LC_SEVERITY_WARNING;
        issue.code = strdup_cpp("MISSING_LAYER_0");
        issue.message = strdup_cpp("Standard layer '0' not found");
        issue.location = strdup_cpp("");
        issues.push_back(issue);
    }

    /* Check entity references */
    std::map<std::string, bool> layerNames;
    for (const auto& l : impl->layers) {
        layerNames[l.name] = true;
    }

    std::map<std::string, bool> blockNames;
    for (const auto& b : impl->blocks) {
        blockNames[b.name] = true;
    }

    int entityIndex = 0;
    for (const auto& e : impl->entities) {
        /* Check layer reference */
        if (!e.layer.empty() && layerNames.find(e.layer) == layerNames.end()) {
            LcValidationIssue issue;
            issue.severity = LC_SEVERITY_ERROR;
            issue.code = strdup_cpp("UNDEFINED_LAYER");
            issue.message = strdup_cpp(("Entity references undefined layer: " + e.layer).c_str());
            issue.location = strdup_cpp(("entity #" + std::to_string(entityIndex)).c_str());
            issues.push_back(issue);
        }

        /* Check block reference for inserts */
        if (e.type == LC_ENTITY_INSERT && !e.blockName.empty()) {
            if (blockNames.find(e.blockName) == blockNames.end()) {
                LcValidationIssue issue;
                issue.severity = LC_SEVERITY_ERROR;
                issue.code = strdup_cpp("UNDEFINED_BLOCK");
                issue.message = strdup_cpp(("Insert references undefined block: " + e.blockName).c_str());
                issue.location = strdup_cpp(("entity #" + std::to_string(entityIndex)).c_str());
                issues.push_back(issue);
            }
        }

        /* Check for invalid geometry */
        if (e.type == LC_ENTITY_CIRCLE || e.type == LC_ENTITY_ARC) {
            if (e.radius <= 0.0) {
                LcValidationIssue issue;
                issue.severity = LC_SEVERITY_ERROR;
                issue.code = strdup_cpp("INVALID_RADIUS");
                issue.message = strdup_cpp("Circle/Arc has invalid radius");
                issue.location = strdup_cpp(("entity #" + std::to_string(entityIndex)).c_str());
                issues.push_back(issue);
            }
        }

        entityIndex++;
    }

    /* Check bounds validity */
    if (impl->minBound.x > impl->maxBound.x) {
        LcValidationIssue issue;
        issue.severity = LC_SEVERITY_INFO;
        issue.code = strdup_cpp("INVALID_BOUNDS");
        issue.message = strdup_cpp("Drawing bounds are invalid (possibly empty drawing)");
        issue.location = strdup_cpp("");
        issues.push_back(issue);
    }

    /* Populate result */
    result->is_valid = 1;
    for (const auto& i : issues) {
        if (i.severity == LC_SEVERITY_ERROR) {
            result->is_valid = 0;
            break;
        }
    }

    result->issue_count = static_cast<int>(issues.size());
    if (!issues.empty()) {
        result->issues = static_cast<LcValidationIssue*>(calloc(issues.size(), sizeof(LcValidationIssue)));
        for (size_t i = 0; i < issues.size(); i++) {
            result->issues[i] = issues[i];
        }
    }

    return result;
}

void lc_validation_result_free(LcValidationResult* result) {
    if (!result) return;

    if (result->issues) {
        for (int i = 0; i < result->issue_count; i++) {
            free(result->issues[i].code);
            free(result->issues[i].message);
            free(result->issues[i].location);
        }
        free(result->issues);
    }

    free(result);
}

char* lc_validation_result_to_json(const LcValidationResult* result) {
    if (!result) return nullptr;

    std::ostringstream json;
    json << "{\n";
    json << "  \"is_valid\": " << (result->is_valid ? "true" : "false") << ",\n";
    json << "  \"issue_count\": " << result->issue_count << ",\n";
    json << "  \"issues\": [\n";

    for (int i = 0; i < result->issue_count; i++) {
        const auto& issue = result->issues[i];
        json << "    {\n";
        json << "      \"severity\": \"" << (issue.severity == LC_SEVERITY_ERROR ? "error" :
                                             issue.severity == LC_SEVERITY_WARNING ? "warning" : "info") << "\",\n";
        json << "      \"code\": \"" << escapeJson(issue.code ? issue.code : "") << "\",\n";
        json << "      \"message\": \"" << escapeJson(issue.message ? issue.message : "") << "\",\n";
        json << "      \"location\": \"" << escapeJson(issue.location ? issue.location : "") << "\"\n";
        json << "    }" << (i < result->issue_count - 1 ? "," : "") << "\n";
    }

    json << "  ]\n";
    json << "}\n";

    return strdup_cpp(json.str());
}

void lc_string_free(char* str) {
    free(str);
}

} /* extern "C" */

/* ============================================================================
 * JWW Export Implementation
 * ============================================================================ */

static LcError lc_document_save_jww(LcDocument* doc, const char* filename) {
    if (!doc || !filename) {
        g_last_error = "Invalid arguments";
        return LC_ERR_INVALID_ARGUMENT;
    }

    auto* impl = reinterpret_cast<DocumentImpl*>(doc);

    /* Create JWW document for writing */
    std::string inFileName = "";  /* Empty input file - we're creating new */
    std::string outFileName = filename;
    JWWDocument jwwDoc(inFileName, outFileName);

    if (!jwwDoc.ofs || !jwwDoc.ofs->good()) {
        g_last_error = "Failed to open JWW file for writing";
        return LC_ERR_WRITE_ERROR;
    }

    /* Initialize header with default values */
    jwwDoc.Header.head = "JwsFileFormat_ver";
    jwwDoc.Header.JW_DATA_VERSION = 800;  /* Version 8.00 format */
    jwwDoc.Header.m_strMemo = "Exported from cadutil";
    jwwDoc.Header.m_nZumen = 2;  /* A3 paper size */
    jwwDoc.Header.m_nWriteGLay = 0;
    jwwDoc.Header.m_dBairitsu = 1.0;
    jwwDoc.Header.m_DPGenten = {0.0, 0.0};

    /* Initialize layer group settings */
    for (int g = 0; g < 16; g++) {
        jwwDoc.Header.GLay[g].m_anGLay = 0;
        jwwDoc.Header.GLay[g].m_anWriteLay = 0;
        jwwDoc.Header.GLay[g].m_adScale = 1.0;
        jwwDoc.Header.GLay[g].m_anGLayProtect = 0;
        for (int l = 0; l < 16; l++) {
            jwwDoc.Header.GLay[g].m_nLay[l].m_aanLay = 0;
            jwwDoc.Header.GLay[g].m_nLay[l].m_aanLayProtect = 0;
        }
    }

    /* Initialize pen settings */
    for (int i = 0; i < 10; i++) {
        jwwDoc.Header.m_Pen[i].m_m_aPenColor = i;
        jwwDoc.Header.m_Pen[i].m_anPenWidth = 1;
        jwwDoc.Header.m_PrtPen[i].m_aPrtpenColor = i;
        jwwDoc.Header.m_PrtPen[i].m_anPrtPenWidth = 1;
        jwwDoc.Header.m_PrtPen[i].m_adPrtTenHankei = 0.5;
    }

    /* Initialize counters */
    jwwDoc.SaveSenCount = 0;
    jwwDoc.SaveEnkoCount = 0;
    jwwDoc.SaveTenCount = 0;
    jwwDoc.SaveMojiCount = 0;
    jwwDoc.SaveSunpouCount = 0;
    jwwDoc.SaveSolidCount = 0;
    jwwDoc.SaveBlockCount = 0;
    jwwDoc.SaveDataListCount = 0;

    /* Convert entities to JWW format */
    for (const auto& e : impl->entities) {
        switch (e.type) {
            case LC_ENTITY_POINT: {
                CDataTen ten;
                ten.SetVersion(800);
                ten.m_start.x = e.point1.x;
                ten.m_start.y = e.point1.y;
                ten.m_bKariten = 0;
                ten.m_nCode = 0;
                ten.m_radKaitenKaku = 0.0;
                ten.m_dBairitsu = 1.0;
                ten.m_lGroup = 0;
                ten.m_nPenStyle = 1;
                ten.m_nPenColor = static_cast<jwWORD>(e.color > 0 && e.color < 10 ? e.color : 1);
                ten.m_nPenWidth = 1;
                ten.m_nLayer = 0;
                ten.m_nGLayer = 0;
                ten.m_sFlg = 0;
                jwwDoc.vTen.push_back(ten);
                jwwDoc.SaveTenCount++;
                break;
            }
            case LC_ENTITY_LINE: {
                CDataSen sen;
                sen.SetVersion(800);
                sen.m_start.x = e.point1.x;
                sen.m_start.y = e.point1.y;
                sen.m_end.x = e.point2.x;
                sen.m_end.y = e.point2.y;
                sen.m_lGroup = 0;
                sen.m_nPenStyle = 1;
                sen.m_nPenColor = static_cast<jwWORD>(e.color > 0 && e.color < 10 ? e.color : 1);
                sen.m_nPenWidth = 1;
                sen.m_nLayer = 0;
                sen.m_nGLayer = 0;
                sen.m_sFlg = 0;
                jwwDoc.vSen.push_back(sen);
                jwwDoc.SaveSenCount++;
                break;
            }
            case LC_ENTITY_CIRCLE: {
                CDataEnko enko;
                enko.SetVersion(800);
                enko.m_start.x = e.point1.x;
                enko.m_start.y = e.point1.y;
                enko.m_dHankei = e.radius;
                enko.m_radKaishiKaku = 0.0;
                enko.m_radEnkoKaku = 2.0 * M_PI;  /* Full circle */
                enko.m_radKatamukiKaku = 0.0;
                enko.m_dHenpeiRitsu = 1.0;
                enko.m_bZenEnFlg = 1;  /* Full circle flag */
                enko.m_lGroup = 0;
                enko.m_nPenStyle = 1;
                enko.m_nPenColor = static_cast<jwWORD>(e.color > 0 && e.color < 10 ? e.color : 1);
                enko.m_nPenWidth = 1;
                enko.m_nLayer = 0;
                enko.m_nGLayer = 0;
                enko.m_sFlg = 0;
                jwwDoc.vEnko.push_back(enko);
                jwwDoc.SaveEnkoCount++;
                break;
            }
            case LC_ENTITY_ARC: {
                CDataEnko enko;
                enko.SetVersion(800);
                enko.m_start.x = e.point1.x;
                enko.m_start.y = e.point1.y;
                enko.m_dHankei = e.radius;
                enko.m_radKaishiKaku = e.startAngle;
                /* Calculate arc angle from start to end */
                double arcAngle = e.endAngle - e.startAngle;
                if (arcAngle < 0) arcAngle += 2.0 * M_PI;
                enko.m_radEnkoKaku = arcAngle;
                enko.m_radKatamukiKaku = 0.0;
                enko.m_dHenpeiRitsu = 1.0;
                enko.m_bZenEnFlg = 0;  /* Not full circle */
                enko.m_lGroup = 0;
                enko.m_nPenStyle = 1;
                enko.m_nPenColor = static_cast<jwWORD>(e.color > 0 && e.color < 10 ? e.color : 1);
                enko.m_nPenWidth = 1;
                enko.m_nLayer = 0;
                enko.m_nGLayer = 0;
                enko.m_sFlg = 0;
                jwwDoc.vEnko.push_back(enko);
                jwwDoc.SaveEnkoCount++;
                break;
            }
            case LC_ENTITY_ELLIPSE: {
                /* JWW uses flattening ratio for ellipses */
                CDataEnko enko;
                enko.SetVersion(800);
                enko.m_start.x = e.point1.x;
                enko.m_start.y = e.point1.y;
                /* Calculate major axis length */
                double majorLen = std::sqrt(e.point2.x * e.point2.x + e.point2.y * e.point2.y);
                enko.m_dHankei = majorLen;
                enko.m_radKaishiKaku = e.startAngle;
                double arcAngle = e.endAngle - e.startAngle;
                if (arcAngle <= 0) arcAngle += 2.0 * M_PI;
                enko.m_radEnkoKaku = arcAngle;
                /* Tilt angle from major axis direction */
                enko.m_radKatamukiKaku = std::atan2(e.point2.y, e.point2.x);
                enko.m_dHenpeiRitsu = e.radius;  /* Ratio stored in radius field */
                enko.m_bZenEnFlg = (arcAngle >= 2.0 * M_PI - 0.001) ? 1 : 0;
                enko.m_lGroup = 0;
                enko.m_nPenStyle = 1;
                enko.m_nPenColor = static_cast<jwWORD>(e.color > 0 && e.color < 10 ? e.color : 1);
                enko.m_nPenWidth = 1;
                enko.m_nLayer = 0;
                enko.m_nGLayer = 0;
                enko.m_sFlg = 0;
                jwwDoc.vEnko.push_back(enko);
                jwwDoc.SaveEnkoCount++;
                break;
            }
            case LC_ENTITY_TEXT:
            case LC_ENTITY_MTEXT: {
                CDataMoji moji;
                moji.SetVersion(800);
                moji.m_start.x = e.point1.x;
                moji.m_start.y = e.point1.y;
                /* Calculate end point based on text length estimation */
                double textLen = e.text.length() * e.height * 0.6;
                moji.m_end.x = e.point1.x + textLen * std::cos(e.rotation);
                moji.m_end.y = e.point1.y + textLen * std::sin(e.rotation);
                moji.m_nMojiShu = 0;
                moji.m_dSizeX = e.height > 0 ? e.height * 0.8 : 2.0;
                moji.m_dSizeY = e.height > 0 ? e.height : 2.5;
                moji.m_dKankaku = 0.0;
                moji.m_degKakudo = e.rotation * 180.0 / M_PI;  /* Convert to degrees */
                moji.m_strFontName = "ＭＳ ゴシック";
                moji.m_string = e.text;
                moji.m_lGroup = 0;
                moji.m_nPenStyle = 1;
                moji.m_nPenColor = static_cast<jwWORD>(e.color > 0 && e.color < 10 ? e.color : 1);
                moji.m_nPenWidth = 1;
                moji.m_nLayer = 0;
                moji.m_nGLayer = 0;
                moji.m_sFlg = 0;
                jwwDoc.vMoji.push_back(moji);
                jwwDoc.SaveMojiCount++;
                break;
            }
            case LC_ENTITY_SOLID: {
                CDataSolid solid;
                solid.SetVersion(800);
                solid.m_start.x = e.point1.x;
                solid.m_start.y = e.point1.y;
                solid.m_end.x = e.point1.x;
                solid.m_end.y = e.point1.y;
                solid.m_DPoint2.x = e.point1.x;
                solid.m_DPoint2.y = e.point1.y;
                solid.m_DPoint3.x = e.point1.x;
                solid.m_DPoint3.y = e.point1.y;
                solid.m_Color = 0;
                solid.m_lGroup = 0;
                solid.m_nPenStyle = 1;
                solid.m_nPenColor = static_cast<jwWORD>(e.color > 0 && e.color < 10 ? e.color : 1);
                solid.m_nPenWidth = 1;
                solid.m_nLayer = 0;
                solid.m_nGLayer = 0;
                solid.m_sFlg = 0;
                jwwDoc.vSolid.push_back(solid);
                jwwDoc.SaveSolidCount++;
                break;
            }
            default:
                /* Skip unsupported entity types */
                break;
        }
    }

    /* Write the JWW file */
    if (!jwwDoc.Save()) {
        g_last_error = "Failed to save JWW file";
        return LC_ERR_WRITE_ERROR;
    }

    return LC_OK;
}
