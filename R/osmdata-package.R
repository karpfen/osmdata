#' Tools to Work with the OpenStreetMap (OSM) Overpass API
#'
#' The Overpass API (or OSM3S) is a read-only API that serves up custom
#' selected parts of the OSM map data. It acts as a database over the web:
#' the client sends a query to the API and gets back the data set that
#' corresponds to the query.\cr
#' \cr
#' Unlike the main API, which is optimized for editing, Overpass API is
#' optimized for data consumers that need a few elements within a glimpse
#' or up to roughly 100 million elements in some minutes, both selected by
#' search criteria like e.g. location, type of objects, tag properties,
#' proximity, or combinations of them. It acts as a database backend for
#' various services.\cr
#' \cr
#' Overpass API has a powerful query language (language guide, language
#' reference, an IDE) beyond XAPI, but also has a compatibility layer to
#' allow a smooth transition from XAPI.
#'
#' @section Functions:
#' \tabular{ll}{
#' \code{add_feature}\tab Add feature to an overpass API query\cr
#' \code{available_features}\tab Obtain list all OpenStreetMap features
#' \code{available_tags}\tab List all tags for a given OpenStreetMap feature
#' \code{bbox_to_string}\tab Convert matrix or vector to a bbox string to pass
#' to overpass API\cr
#' \code{issue_query}\tab Finalize and issue an Overpass query\cr
#' \code{opq}\tab Begin building an Overpass query\cr
#' \code{overpass_query}\tab Issue OSM Overpass Query\cr
#' \code{overpass_status}\tab Retrieve status of the Overpass API\cr
#' \code{read_osm}\tab Read an XML OSM Overpass response from path\cr
#' }
#' @name osmdata
#' @docType package
#' @author Bob Rudis, Robin Lovelace, Maelle Salmon, Mark Padgham
#' @import fastmatch pbapply sp xml2 httr tidyr lubridate
#' @importFrom rvest html_attr html_nodes
#' @importFrom dplyr %>% select left_join filter arrange bind_rows mutate do group_by distinct data_frame
#' @importFrom utils read.table
#' @importFrom curl has_internet
#' @importFrom Rcpp evalCpp
#' @useDynLib osmdata
NULL


#' osmdata exported operators
#'
#' The following functions are imported and then re-exported
#' from the osmdata package to enable use of the magrittr/dplyr
#' pipe operator with no additional library calls
#'
#' @name osmdata-exports
NULL

#' @importFrom dplyr %>%
#' @name %>%
#' @export
#' @rdname osmdata-exports
NULL