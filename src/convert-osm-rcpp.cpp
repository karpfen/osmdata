/***************************************************************************
 *  Project:    osmdata
 *  File:       convert_osm_rcpp.cpp
 *  Language:   C++
 *
 *  osmdata is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  osmdata is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  osm-router.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Author:     Mark Padgham 
 *  E-Mail:     mark.padgham@email.com 
 *
 *  Description:    Functions to convert OSM data stored in C++ arrays to Rcpp
 *                  structures.
 *
 *  Limitations:
 *
 *  Dependencies:       none (rapidXML header included in osmdatar)
 *
 *  Compiler Options:   -std=c++11
 ***************************************************************************/

#include "convert-osm-rcpp.h"


/************************************************************************
 ************************************************************************
 **                                                                    **
 **       FUNCTIONS TO CONVERT C++ OBJECTS TO Rcpp::List OBJECTS       **
 **                                                                    **
 ************************************************************************
 ************************************************************************/

/* Traces a single way and adds (lon,lat,rownames) to an Rcpp::NumericMatrix
 *
 * @param &ways pointer to Ways structure
 * @param &nodes pointer to Nodes structure
 * @param first_node Last node of previous way to find in current
 * @param &wayi_id pointer to ID of current way
 * @lons pointer to vector of longitudes
 * @lats pointer to vector of latitutdes
 * @rownames pointer to vector of rownames for each node.
 * @nmat Rcpp::NumericMatrix to store lons, lats, and rownames
 */
void trace_way_nmat (const Ways &ways, const Nodes &nodes, 
        const osmid_t &wayi_id, Rcpp::NumericMatrix &nmat)
{
    auto wayi = ways.find (wayi_id);
    std::vector <std::string> rownames;
    rownames.clear ();
    int n = wayi->second.nodes.size ();
    rownames.reserve (n);
    nmat = Rcpp::NumericMatrix (Rcpp::Dimension (n, 2));

    int tempi = 0;
    for (auto ni = wayi->second.nodes.begin ();
            ni != wayi->second.nodes.end (); ++ni)
    {
        rownames.push_back (std::to_string (*ni));
        nmat (tempi, 0) = nodes.find (*ni)->second.lon;
        nmat (tempi++, 1) = nodes.find (*ni)->second.lat;
    }

    std::vector <std::string> colnames = {"lon", "lat"};
    Rcpp::List dimnames (0);
    dimnames.push_back (rownames);
    dimnames.push_back (colnames);
    nmat.attr ("dimnames") = dimnames;
    dimnames.erase (0, dimnames.size ());
}

/* get_value_mat_way
 *
 * Extract key-value pairs for a given way and fill Rcpp::CharacterMatrix
 *
 * @param wayi Constant iterator to one OSM way
 * @param Ways Pointer to the std::vector of all ways
 * @param unique_vals Pointer to the UniqueVals structure
 * @param value_arr Pointer to the Rcpp::CharacterMatrix of values to be filled
 *        by tracing the key-val pairs of the way 'wayi'
 * @param rowi Integer value for the key-val pairs for wayi
 */
// TODO: Is it faster to use a std::vector <std::string> instead of
// Rcpp::CharacterMatrix and then simply
// Rcpp::CharacterMatrix mat (nrow, ncol, value_vec.begin ()); ?
void get_value_mat_way (Ways::const_iterator wayi, const Ways &ways,
        const UniqueVals &unique_vals, Rcpp::CharacterMatrix &value_arr, int rowi)
{
    for (auto kv_iter = wayi->second.key_val.begin ();
            kv_iter != wayi->second.key_val.end (); ++kv_iter)
    {
        const std::string &key = kv_iter->first;
        int coli = std::distance (unique_vals.k_way.begin (),
                unique_vals.k_way.find (key));
        value_arr (rowi, coli) = kv_iter->second;
    }
}

/* get_value_mat_rel
 *
 * Extract key-value pairs for a given relation and fill Rcpp::CharacterMatrix
 *
 * @param reli Constant iterator to one OSM relation
 * @param rels Pointer to the std::vector of all relations
 * @param unique_vals Pointer to the UniqueVals structure
 * @param value_arr Pointer to the Rcpp::CharacterMatrix of values to be filled
 *        by tracing the key-val pairs of the relation 'reli'
 * @param rowi Integer value for the key-val pairs for reli
 */
void get_value_mat_rel (Relations::const_iterator &reli, const Relations &rels,
        const UniqueVals &unique_vals, Rcpp::CharacterMatrix &value_arr, int rowi)
{
    for (auto kv_iter = reli->key_val.begin (); kv_iter != reli->key_val.end ();
            ++kv_iter)
    {
        const std::string &key = kv_iter->first;
        int coli = std::distance (unique_vals.k_rel.begin (),
                unique_vals.k_rel.find (key));
        value_arr (rowi, coli) = kv_iter->second;
    }
}


/* restructure_kv_mat
 *
 * Restructures a key-value matrix to reflect typical GDAL output by
 * inserting a first column containing "osm_id" and moving the "name" column to
 * second position.
 *
 * @param kv Pointer to the key-value matrix to be restructured
 * @param ls true only for multilinestrings, which have compound rownames which
 *        include roles
 */
Rcpp::CharacterMatrix restructure_kv_mat (Rcpp::CharacterMatrix &kv, bool ls=false)
{
    // The following has to be done in 2 lines:
    std::vector <std::vector <std::string> > dims = kv.attr ("dimnames");
    std::vector <std::string> ids = dims [0], varnames = dims [1], varnames_new;
    Rcpp::CharacterMatrix kv_out;

    unsigned ni = std::distance (varnames.begin (),
            std::find (varnames.begin (), varnames.end (), "name"));
    unsigned add_lines = 1;
    if (ls)
        add_lines++;

    if (ni < varnames.size ())
    {
        Rcpp::CharacterVector name_vals = kv.column (ni);
        Rcpp::CharacterVector roles; // only for ls, but has to be defined here
        // convert ids to CharacterVector - direct allocation doesn't work
        Rcpp::CharacterVector ids_rcpp (ids.size ());
        if (!ls)
            for (unsigned i=0; i<ids.size (); i++)
                ids_rcpp (i) = ids [i];
        else
        { // extract way roles for multilinestring kev-val matrices
            roles = Rcpp::CharacterVector (ids.size ());
            for (unsigned i=0; i<ids.size (); i++)
            {
                int ipos = ids [i].find ("-", 0);
                ids_rcpp (i) = ids [i].substr (0, ipos).c_str ();
                roles (i) = ids [i].substr (ipos + 1, ids[i].length () - ipos);
            }
        }

        varnames_new.reserve (kv.ncol () + add_lines);
        varnames_new.push_back ("osm_id");
        varnames_new.push_back ("name");
        kv_out = Rcpp::CharacterMatrix (Rcpp::Dimension (kv.nrow (),
                    kv.ncol () + add_lines));
        kv_out.column (0) = ids_rcpp;
        kv_out.column (1) = name_vals;
        if (ls)
        {
            varnames_new.push_back ("role");
            kv_out.column (2) = roles;
        }
        unsigned count = 1 + add_lines;
        for (unsigned i=0; i<(unsigned) kv.ncol (); i++)
            if (i != ni)
            {
                varnames_new.push_back (varnames [i]);
                kv_out.column (count++) = kv.column (i);
            }
        kv_out.attr ("dimnames") = Rcpp::List::create (ids, varnames_new);
    } else
        kv_out = kv;

    return kv_out;
}

/* convert_poly_linestring_to_sf
 *
 * Converts the data contained in all the arguments into an Rcpp::List object
 * to be used as the geometry column of a Simple Features Colletion
 *
 * @param lon_arr 3D array of longitudinal coordinates
 * @param lat_arr 3D array of latgitudinal coordinates
 * @param rowname_arr 3D array of <osmid_t> IDs for nodes of all (lon,lat)
 * @param id_vec 2D array of either <std::string> or <osmid_t> IDs for all ways
 *        used in the geometry.
 * @param rel_id Vector of <osmid_t> IDs for each relation.
 *
 * @return An Rcpp::List object of [relation][way][node/geom] data.
 */
// TODO: Replace return value with pointer to List as argument?
template <typename T> Rcpp::List convert_poly_linestring_to_sf (
        const float_arr3 &lon_arr, const float_arr3 &lat_arr, 
        const string_arr3 &rowname_arr, 
        const std::vector <std::vector <T> > &id_vec, 
        const std::vector <std::string> &rel_id, const std::string type)
{
    if (!(type == "MULTILINESTRING" || type == "MULTIPOLYGON"))
        throw std::runtime_error ("type must be multilinestring/polygon");
    Rcpp::List outList (lon_arr.size ()); 
    Rcpp::NumericMatrix nmat (Rcpp::Dimension (0, 0));
    Rcpp::List dimnames (0);
    std::vector <std::string> colnames = {"lat", "lon"};
    for (unsigned i=0; i<lon_arr.size (); i++) // over all relations
    {
        Rcpp::List outList_i (lon_arr [i].size ()); 
        for (unsigned j=0; j<lon_arr [i].size (); j++) // over all ways
        {
            unsigned n = lon_arr [i][j].size ();
            nmat = Rcpp::NumericMatrix (Rcpp::Dimension (n, 2));
            std::copy (lon_arr [i][j].begin (), lon_arr [i][j].end (),
                    nmat.begin ());
            std::copy (lat_arr [i][j].begin (), lat_arr [i][j].end (),
                    nmat.begin () + n);
            dimnames.push_back (rowname_arr [i][j]);
            dimnames.push_back (colnames);
            nmat.attr ("dimnames") = dimnames;
            dimnames.erase (0, dimnames.size ());
            outList_i [j] = nmat;
        }
        outList_i.attr ("names") = id_vec [i];
        if (type == "MULTIPOLYGON")
        {
            Rcpp::List tempList (1);
            tempList (0) = outList_i;
            tempList.attr ("class") = Rcpp::CharacterVector::create ("XY", type, "sfg");
            outList [i] = tempList;
        } else
        {
            outList_i.attr ("class") = Rcpp::CharacterVector::create ("XY", type, "sfg");
            outList [i] = outList_i;
        }
    }
    outList.attr ("names") = rel_id;

    return outList;
}
template Rcpp::List convert_poly_linestring_to_sf <osmid_t> (
        const float_arr3 &lon_arr, const float_arr3 &lat_arr, 
        const string_arr3 &rowname_arr, 
        const std::vector <std::vector <osmid_t> > &id_vec, 
        const std::vector <std::string> &rel_id, const std::string type);
template Rcpp::List convert_poly_linestring_to_sf <std::string> (
        const float_arr3 &lon_arr, const float_arr3 &lat_arr, 
        const string_arr3 &rowname_arr, 
        const std::vector <std::vector <std::string> > &id_vec, 
        const std::vector <std::string> &rel_id, const std::string type);

/* convert_multipoly_to_sp
 *
 * Converts the data contained in all the arguments into a
 * SpatialPolygonsDataFrame
 *
 * @param lon_arr 3D array of longitudinal coordinates
 * @param lat_arr 3D array of latgitudinal coordinates
 * @param rowname_arr 3D array of <osmid_t> IDs for nodes of all (lon,lat)
 * @param id_vec 2D array of either <std::string> or <osmid_t> IDs for all ways
 *        used in the geometry.
 * @param rel_id Vector of <osmid_t> IDs for each relation.
 *
 * @return Object pointed to by 'multipolygons' is constructed.
 */
void convert_multipoly_to_sp (Rcpp::S4 &multipolygons, const Relations &rels,
        const float_arr3 &lon_arr, const float_arr3 &lat_arr, 
        const string_arr3 &rowname_arr, const string_arr2 &id_vec,
        const UniqueVals &unique_vals)
{

    Rcpp::Environment sp_env = Rcpp::Environment::namespace_env ("sp");
    Rcpp::Function Polygon = sp_env ["Polygon"];
    Rcpp::Language polygons_call ("new", "Polygons");

    int nrow = lon_arr.size (), ncol = unique_vals.k_rel.size ();
    Rcpp::CharacterMatrix kv_mat (Rcpp::Dimension (nrow, ncol));
    std::fill (kv_mat.begin (), kv_mat.end (), NA_STRING);

    Rcpp::List outList (lon_arr.size ()); 
    Rcpp::NumericMatrix nmat (Rcpp::Dimension (0, 0));
    Rcpp::List dimnames (0);
    std::vector <std::string> colnames = {"lat", "lon"}, rel_id;

    unsigned npolys = 0;
    for (auto itr = rels.begin (); itr != rels.end (); itr++)
        if (itr->ispoly)
            npolys++;
    if (npolys != lon_arr.size ())
        throw std::runtime_error ("polygons must be same size as geometries");
    rel_id.reserve (npolys);

    int i = 0;
    for (auto itr = rels.begin (); itr != rels.end (); itr++)
        if (itr->ispoly)
        {
            Rcpp::List outList_i (lon_arr [i].size ()); 
            // j over all ways, with outer always first followed by inner
            bool outer = true;
            for (unsigned j=0; j<lon_arr [i].size (); j++) 
            {
                int n = lon_arr [i][j].size ();
                nmat = Rcpp::NumericMatrix (Rcpp::Dimension (n, 2));
                std::copy (lon_arr [i][j].begin (), lon_arr [i][j].end (),
                        nmat.begin ());
                std::copy (lat_arr [i][j].begin (), lat_arr [i][j].end (),
                        nmat.begin () + n);
                dimnames.push_back (rowname_arr [i][j]);
                dimnames.push_back (colnames);
                nmat.attr ("dimnames") = dimnames;
                dimnames.erase (0, dimnames.size ());

                Rcpp::S4 poly = Polygon (nmat);
                poly.slot ("hole") = !outer;
                poly.slot ("ringDir") = (int) 1; // TODO: Check this!
                if (outer)
                    outer = false;
                else
                    poly.slot ("ringDir") = (int) -1; // TODO: Check this!
                outList_i [j] = poly;
            }
            outList_i.attr ("names") = id_vec [i];

            Rcpp::S4 polygons = polygons_call.eval ();
            polygons.slot ("Polygons") = outList_i;
            polygons.slot ("ID") = id_vec [i];
            polygons.slot ("plotOrder") = (int) i + 1; 
            //polygons.slot ("labpt") = poly.slot ("labpt");
            //polygons.slot ("area") = poly.slot ("area");
            outList [i] = polygons;
            rel_id.push_back (std::to_string (itr->id));

            get_value_mat_rel (itr, rels, unique_vals, kv_mat, i++);
        } // end if ispoly & for i
    outList.attr ("names") = rel_id;

    Rcpp::Language sp_polys_call ("new", "SpatialPolygonsDataFrame");
    multipolygons = sp_polys_call.eval ();
    multipolygons.slot ("polygons") = outList;
    // fill plotOrder with numeric vector
    std::vector <int> plotord;
    for (unsigned i=0; i<rels.size (); i++) plotord.push_back (i + 1);
    multipolygons.slot ("plotOrder") = plotord;
    plotord.clear ();

    Rcpp::DataFrame kv_df;
    if (rel_id.size () > 0)
    {
        kv_mat.attr ("names") = unique_vals.k_rel;
        kv_mat.attr ("dimnames") = Rcpp::List::create (rel_id, unique_vals.k_rel);
        kv_df = kv_mat;
        kv_df.attr ("names") = unique_vals.k_rel;
        multipolygons.slot ("data") = kv_df;
    }
    rel_id.clear ();
}


/* convert_multiline_to_sp
 *
 * Converts the data contained in all the arguments into a SpatialLinesDataFrame
 *
 * @param lon_arr 3D array of longitudinal coordinates
 * @param lat_arr 3D array of latgitudinal coordinates
 * @param rowname_arr 3D array of <osmid_t> IDs for nodes of all (lon,lat)
 * @param id_vec 2D array of either <std::string> or <osmid_t> IDs for all ways
 *        used in the geometry.
 * @param rel_id Vector of <osmid_t> IDs for each relation.
 *
 * @return Object pointed to by 'multilines' is constructed.
 */
void convert_multiline_to_sp (Rcpp::S4 &multilines, const Relations &rels,
        const float_arr3 &lon_arr, const float_arr3 &lat_arr, 
        const string_arr3 &rowname_arr, const osmt_arr2 &id_vec,
        const UniqueVals &unique_vals)
{

    Rcpp::Language line_call ("new", "Line");
    Rcpp::Language lines_call ("new", "Lines");

    Rcpp::NumericMatrix nmat (Rcpp::Dimension (0, 0));
    Rcpp::List dimnames (0);
    std::vector <std::string> colnames = {"lat", "lon"}, rel_id;

    int nlines = 0;
    for (auto itr = rels.begin (); itr != rels.end (); itr++)
        if (!itr->ispoly)
            nlines++;
    rel_id.reserve (nlines);

    Rcpp::List outList (nlines); 
    int ncol = unique_vals.k_rel.size ();
    Rcpp::CharacterMatrix kv_mat (Rcpp::Dimension (nlines, ncol));
    std::fill (kv_mat.begin (), kv_mat.end (), NA_STRING);

    int i = 0;
    for (auto itr = rels.begin (); itr != rels.end (); itr++)
        if (!itr->ispoly)
        {
            Rcpp::List outList_i (lon_arr [i].size ()); 
            // j over all ways
            for (unsigned j=0; j<lon_arr [i].size (); j++) 
            {
                unsigned n = lon_arr [i][j].size ();
                nmat = Rcpp::NumericMatrix (Rcpp::Dimension (n, 2));
                std::copy (lon_arr [i][j].begin (), lon_arr [i][j].end (),
                        nmat.begin ());
                std::copy (lat_arr [i][j].begin (), lat_arr [i][j].end (),
                        nmat.begin () + n);
                dimnames.push_back (rowname_arr [i][j]);
                dimnames.push_back (colnames);
                nmat.attr ("dimnames") = dimnames;
                dimnames.erase (0, dimnames.size ());

                Rcpp::S4 line = line_call.eval ();
                line.slot ("coords") = nmat;

                outList_i [j] = line;
            }
            outList_i.attr ("names") = id_vec [i]; // implicit type conversion
            Rcpp::S4 lines = lines_call.eval ();
            lines.slot ("Lines") = outList_i;
            lines.slot ("ID") = itr->id;

            outList [i] = lines;
            rel_id.push_back (std::to_string (itr->id));

            get_value_mat_rel (itr, rels, unique_vals, kv_mat, i++);
        } // end if ispoly & for i
    outList.attr ("names") = rel_id;

    Rcpp::Language sp_lines_call ("new", "SpatialLinesDataFrame");
    multilines = sp_lines_call.eval ();
    multilines.slot ("lines") = outList;

    Rcpp::DataFrame kv_df;
    if (rel_id.size () > 0)
    {
        kv_mat.attr ("names") = unique_vals.k_rel;
        kv_mat.attr ("dimnames") = Rcpp::List::create (rel_id, unique_vals.k_rel);
        kv_df = kv_mat;
        kv_df.attr ("names") = unique_vals.k_rel;
        multilines.slot ("data") = kv_df;
    }
    rel_id.clear ();
}
