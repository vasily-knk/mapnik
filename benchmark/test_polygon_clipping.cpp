#include "bench_framework.hpp"
#include "compare_images.hpp"
#include <mapnik/geometry.hpp>
#include <mapnik/vertex.hpp>
#include <mapnik/transform_path_adapter.hpp>
#include <mapnik/view_transform.hpp>
#include <mapnik/graphics.hpp>
#include <mapnik/wkt/wkt_factory.hpp>
#include <mapnik/projection.hpp>
#include <mapnik/proj_transform.hpp>
#include <mapnik/util/fs.hpp>
#include <mapnik/polygon_clipper.hpp>
#include <mapnik/image_util.hpp>
// agg
#include "agg_conv_clip_polygon.h"
// clipper
#include "agg_conv_clipper.h"
#include "agg_path_storage.h"
// rendering
#include "agg_basics.h"
#include "agg_rendering_buffer.h"
#include "agg_pixfmt_rgba.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_scanline_u.h"
#include "agg_renderer_scanline.h"

// stl
#include <fstream>
#include <iostream>
#include <cstdlib>

void render(mapnik::geometry_type const& geom,
            mapnik::box2d<double> const& extent,
            std::string const& name)
{
    using path_type = mapnik::transform_path_adapter<mapnik::view_transform,mapnik::vertex_adapter>;
    using ren_base = agg::renderer_base<agg::pixfmt_rgba32_plain>;
    using renderer = agg::renderer_scanline_aa_solid<ren_base>;
    mapnik::vertex_adapter va(geom);
    mapnik::image_32 im(256,256);
    im.set_background(mapnik::color("white"));
    mapnik::box2d<double> padded_extent = extent;
    padded_extent.pad(10);
    mapnik::view_transform tr(im.width(),im.height(),padded_extent,0,0);
    agg::rendering_buffer buf(im.raw_data(),im.width(),im.height(), im.width() * 4);
    agg::pixfmt_rgba32_plain pixf(buf);
    ren_base renb(pixf);
    renderer ren(renb);
    ren.color(agg::rgba8(127,127,127,255));
    agg::rasterizer_scanline_aa<> ras;
    mapnik::proj_transform prj_trans(mapnik::projection("+init=epsg:4326"),mapnik::projection("+init=epsg:4326"));
    path_type path(tr,va,prj_trans);
    ras.add_path(path);
    agg::scanline_u8 sl;
    agg::render_scanlines(ras, sl, ren);
    mapnik::save_to_file(im.data(),name);
}

class test1 : public benchmark::test_case
{
    std::string wkt_in_;
    mapnik::box2d<double> extent_;
    std::string expected_;
public:
    using conv_clip = agg::conv_clip_polygon<mapnik::vertex_adapter>;
    test1(mapnik::parameters const& params,
          std::string const& wkt_in,
          mapnik::box2d<double> const& extent)
     : test_case(params),
       wkt_in_(wkt_in),
       extent_(extent),
       expected_("./benchmark/data/polygon_clipping_agg") {}
    bool validate() const
    {
        std::string expected_wkt("Polygon((181 286.666667,233 454,315 340,421 446,463 324,559 466,631 321.320755,631 234.386861,528 178,394 229,329 138,212 134,183 228,200 264,181 238.244444),(313 190,440 256,470 248,510 305,533 237,613 263,553 397,455 262,405 378,343 287,249 334,229 191,313 190,313 190))");
        boost::ptr_vector<mapnik::geometry_type> paths;
        if (!mapnik::from_wkt(wkt_in_, paths))
        {
            throw std::runtime_error("Failed to parse WKT");
        }
        if (paths.size() != 1)
        {
            std::clog << "paths.size() != 1\n";
            return false;
        }
        mapnik::geometry_type const& geom = paths[0];
        mapnik::vertex_adapter va(geom);
        conv_clip clipped(va);
        clipped.clip_box(
                    extent_.minx(),
                    extent_.miny(),
                    extent_.maxx(),
                    extent_.maxy());
        unsigned cmd;
        double x,y;
        clipped.rewind(0);
        mapnik::geometry_type geom2(mapnik::geometry_type::types::Polygon);
        while ((cmd = clipped.vertex(&x, &y)) != mapnik::SEG_END) {
            geom2.push_vertex(x,y,(mapnik::CommandType)cmd);
        }
        std::string expect = expected_+".png";
        std::string actual = expected_+"_actual.png";
        auto env = mapnik::envelope(geom);
        if (!mapnik::util::exists(expect) || (std::getenv("UPDATE") != nullptr))
        {
            std::clog << "generating expected image: " << expect << "\n";
            render(geom2,env,expect);
        }
        render(geom2,env,actual);
        return benchmark::compare_images(actual,expect);
    }
    bool operator()() const
    {
        boost::ptr_vector<mapnik::geometry_type> paths;
        if (!mapnik::from_wkt(wkt_in_, paths))
        {
            throw std::runtime_error("Failed to parse WKT");
        }
        bool valid = true;
        for (unsigned i=0;i<iterations_;++i)
        {
            unsigned count = 0;
            for (mapnik::geometry_type const& geom : paths)
            {
                mapnik::vertex_adapter va(geom);
                conv_clip clipped(va);
                clipped.clip_box(
                            extent_.minx(),
                            extent_.miny(),
                            extent_.maxx(),
                            extent_.maxy());
                unsigned cmd;
                double x,y;
                // NOTE: this rewind is critical otherwise
                // agg_conv_adapter_vpgen will give garbage
                // values for the first vertex
                clipped.rewind(0);
                while ((cmd = clipped.vertex(&x, &y)) != mapnik::SEG_END) {
                    count++;
                }
            }
            unsigned expected_count = 31;
            if (count != expected_count) {
                std::clog << "test1: clipping failed: processed " << count << " verticies but expected " << expected_count << "\n";
                valid = false;
            }
        }
        return valid;
    }
};

class test2 : public benchmark::test_case
{
    std::string wkt_in_;
    mapnik::box2d<double> extent_;
    std::string expected_;
public:
    using poly_clipper = agg::conv_clipper<mapnik::vertex_adapter, agg::path_storage>;
    test2(mapnik::parameters const& params,
          std::string const& wkt_in,
          mapnik::box2d<double> const& extent)
     : test_case(params),
       wkt_in_(wkt_in),
       extent_(extent),
       expected_("./benchmark/data/polygon_clipping_clipper") {}
    bool validate() const
    {
        std::string expected_wkt("Polygon((212 134,329 138,394 229,528 178,631 234.4,631 321.3,559 466,463 324,421 446,315 340,233 454,181 286.7,181 238.2,200 264,183 228),(313 190,229 191,249 334,343 287,405 378,455 262,553 397,613 263,533 237,510 305,470 248,440 256))");
        boost::ptr_vector<mapnik::geometry_type> paths;
        if (!mapnik::from_wkt(wkt_in_, paths))
        {
            throw std::runtime_error("Failed to parse WKT");
        }
        agg::path_storage ps;
        ps.move_to(extent_.minx(), extent_.miny());
        ps.line_to(extent_.minx(), extent_.maxy());
        ps.line_to(extent_.maxx(), extent_.maxy());
        ps.line_to(extent_.maxx(), extent_.miny());
        ps.close_polygon();
        if (paths.size() != 1)
        {
            std::clog << "paths.size() != 1\n";
            return false;
        }
        mapnik::geometry_type const& geom = paths[0];
        mapnik::vertex_adapter va(geom);
        poly_clipper clipped(va,ps,
                             agg::clipper_and,
                             agg::clipper_non_zero,
                             agg::clipper_non_zero,
                             1);
        clipped.rewind(0);
        unsigned cmd;
        double x,y;
        mapnik::geometry_type geom2(mapnik::geometry_type::types::Polygon);
        while ((cmd = clipped.vertex(&x, &y)) != mapnik::SEG_END) {
            geom2.push_vertex(x,y,(mapnik::CommandType)cmd);
        }
        std::string expect = expected_+".png";
        std::string actual = expected_+"_actual.png";
        auto env = mapnik::envelope(geom);
        if (!mapnik::util::exists(expect) || (std::getenv("UPDATE") != nullptr))
        {
            std::clog << "generating expected image: " << expect << "\n";
            render(geom2,env,expect);
        }
        render(geom2,env,actual);
        return benchmark::compare_images(actual,expect);
    }
    bool operator()() const
    {
        boost::ptr_vector<mapnik::geometry_type> paths;
        if (!mapnik::from_wkt(wkt_in_, paths))
        {
            throw std::runtime_error("Failed to parse WKT");
        }
        agg::path_storage ps;
        ps.move_to(extent_.minx(), extent_.miny());
        ps.line_to(extent_.minx(), extent_.maxy());
        ps.line_to(extent_.maxx(), extent_.maxy());
        ps.line_to(extent_.maxx(), extent_.miny());
        ps.close_polygon();
        bool valid = true;
        for (unsigned i=0;i<iterations_;++i)
        {
            unsigned count = 0;
            for (mapnik::geometry_type const& geom : paths)
            {
                mapnik::vertex_adapter va(geom);
                poly_clipper clipped(va,ps,
                                     agg::clipper_and,
                                     agg::clipper_non_zero,
                                     agg::clipper_non_zero,
                                     1);
                clipped.rewind(0);
                unsigned cmd;
                double x,y;
                while ((cmd = clipped.vertex(&x, &y)) != mapnik::SEG_END) {
                    count++;
                }
            }
            unsigned expected_count = 29;
            if (count != expected_count) {
                std::clog << "test1: clipping failed: processed " << count << " verticies but expected " << expected_count << "\n";
                valid = false;
            }
        }
        return valid;
    }
};

class test3 : public benchmark::test_case
{
    std::string wkt_in_;
    mapnik::box2d<double> extent_;
    std::string expected_;
public:
    using poly_clipper = mapnik::polygon_clipper<mapnik::vertex_adapter>;
    test3(mapnik::parameters const& params,
          std::string const& wkt_in,
          mapnik::box2d<double> const& extent)
     : test_case(params),
       wkt_in_(wkt_in),
       extent_(extent),
       expected_("./benchmark/data/polygon_clipping_boost") {}
    bool validate() const
    {
        std::string expected_wkt("Polygon((181 286.666667,233 454,315 340,421 446,463 324,559 466,631 321.320755,631 234.386861,528 178,394 229,329 138,212 134,183 228,200 264,181 238.244444,181 286.666667),(313 190,440 256,470 248,510 305,533 237,613 263,553 397,455 262,405 378,343 287,249 334,229 191,313 190))");
        boost::ptr_vector<mapnik::geometry_type> paths;
        if (!mapnik::from_wkt(wkt_in_, paths))
        {
            throw std::runtime_error("Failed to parse WKT");
        }
        if (paths.size() != 1)
        {
            std::clog << "paths.size() != 1\n";
            return false;
        }
        mapnik::geometry_type const& geom = paths[0];
        mapnik::vertex_adapter va(geom);
        poly_clipper clipped(extent_, va);
        unsigned cmd;
        double x,y;
        mapnik::geometry_type geom2(mapnik::geometry_type::types::Polygon);
        while ((cmd = clipped.vertex(&x, &y)) != mapnik::SEG_END) {
            geom2.push_vertex(x,y,(mapnik::CommandType)cmd);
        }
        std::string expect = expected_+".png";
        std::string actual = expected_+"_actual.png";
        auto env = mapnik::envelope(geom);
        if (!mapnik::util::exists(expect) || (std::getenv("UPDATE") != nullptr))
        {
            std::clog << "generating expected image: " << expect << "\n";
            render(geom2,env,expect);
        }
        render(geom2,env,actual);
        return benchmark::compare_images(actual,expect);
    }
    bool operator()() const
    {
        boost::ptr_vector<mapnik::geometry_type> paths;
        if (!mapnik::from_wkt(wkt_in_, paths))
        {
            throw std::runtime_error("Failed to parse WKT");
        }
        bool valid = true;
        for (unsigned i=0;i<iterations_;++i)
        {
            unsigned count = 0;
            for ( mapnik::geometry_type const& geom : paths)
            {
                mapnik::vertex_adapter va(geom);
                poly_clipper clipped(extent_, va);
                unsigned cmd;
                double x,y;
                while ((cmd = clipped.vertex(&x, &y)) != mapnik::SEG_END) {
                    count++;
                }
            }
            unsigned expected_count = 31;
            if (count != expected_count) {
                std::clog << "test1: clipping failed: processed " << count << " verticies but expected " << expected_count << "\n";
                valid = false;
            }
        }
        return valid;
    }
};

int main(int argc, char** argv)
{
    mapnik::parameters params;
    benchmark::handle_args(argc,argv,params);

    // polygon/rect clipping
    // IN : POLYGON ((155 203, 233 454, 315 340, 421 446, 463 324, 559 466, 665 253, 528 178, 394 229, 329 138, 212 134, 183 228, 200 264, 155 203),(313 190, 440 256, 470 248, 510 305, 533 237, 613 263, 553 397, 455 262, 405 378, 343 287, 249 334, 229 191, 313 190))
    // RECT : POLYGON ((181 106, 181 470, 631 470, 631 106, 181 106))
    // OUT (expected)
    // POLYGON ((181 286.6666666666667, 233 454, 315 340, 421 446, 463 324, 559 466, 631 321.3207547169811, 631 234.38686131386862, 528 178, 394 229, 329 138, 212 134, 183 228, 200 264, 181 238.24444444444444, 181 286.6666666666667),(313 190, 440 256, 470 248, 510 305, 533 237, 613 263, 553 397, 455 262, 405 378, 343 287, 249 334, 229 191, 313 190))

    mapnik::box2d<double> clipping_box(181,106,631,470);
    std::string filename_("./benchmark/data/polygon.wkt");
    std::ifstream in(filename_.c_str(),std::ios_base::in | std::ios_base::binary);
    if (!in.is_open())
        throw std::runtime_error("could not open: '" + filename_ + "'");
    std::string wkt_in( (std::istreambuf_iterator<char>(in) ),
               (std::istreambuf_iterator<char>()) );
    {
        test1 test_runner(params,wkt_in,clipping_box);
        run(test_runner,"clipping polygon with agg");
    }
    {
        test2 test_runner(params,wkt_in,clipping_box);
        run(test_runner,"clipping polygon with clipper");
    }
    {
        test3 test_runner(params,wkt_in,clipping_box);
        run(test_runner,"clipping polygon with boost");
    }

    return 0;
}
