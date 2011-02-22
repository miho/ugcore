//	created by Andreas Vogel

#ifndef __H__LIB_DISCRETIZATION__DOMAIN_UTIL_GENERAL_IMPL__
#define __H__LIB_DISCRETIZATION__DOMAIN_UTIL_GENERAL_IMPL__

#include <string>
#include "lib_grid/tools/subset_handler_interface.h"
#include "lib_grid/tools/subset_handler_multi_grid.h"
#include "lib_grid/tools/subset_handler_grid.h"
#include "./domain_util.h"
#include "lib_discretization/reference_element/reference_element.h"

namespace ug{

/// returns if a subset is a regular grid
inline bool SubsetIsRegularGrid(const SubsetHandler& sh, int si)
{
//	check for constraining/constrained elements
	if(sh.num<HangingVertex>(si) > 0) return false;
	if(sh.num<ConstrainedEdge>(si) > 0) return false;
	if(sh.num<ConstrainingEdge>(si) > 0) return false;
	if(sh.num<ConstrainedTriangle>(si) > 0) return false;
	if(sh.num<ConstrainingTriangle>(si) > 0) return false;
	if(sh.num<ConstrainedQuadrilateral>(si) > 0) return false;
	if(sh.num<ConstrainingQuadrilateral>(si) > 0) return false;

//	if not found, subset describes a regular grid
	return true;
}

/// returns if a subset is a regular grid
inline bool SubsetIsRegularGrid(const MGSubsetHandler& sh, int si)
{
//	check for constraining/constrained elements
	if(sh.num<HangingVertex>(si) > 0) return false;
	if(sh.num<ConstrainedEdge>(si) > 0) return false;
	if(sh.num<ConstrainingEdge>(si) > 0) return false;
	if(sh.num<ConstrainedTriangle>(si) > 0) return false;
	if(sh.num<ConstrainingTriangle>(si) > 0) return false;
	if(sh.num<ConstrainedQuadrilateral>(si) > 0) return false;
	if(sh.num<ConstrainingQuadrilateral>(si) > 0) return false;

//	if not found, subset describes a regular grid
	return true;
}

/// returns if a subset is a regular grid
inline bool SubsetIsRegularGrid(const ISubsetHandler& ish, int si)
{
//	test SubsetHandler
	const SubsetHandler* sh = dynamic_cast<const SubsetHandler*>(&ish);
	if(sh != NULL)
		return SubsetIsRegularGrid(*sh, si);

//	test MGSubsetHandler
	const MGSubsetHandler* mgsh = dynamic_cast<const MGSubsetHandler*>(&ish);
	if(mgsh != NULL)
		return SubsetIsRegularGrid(*mgsh, si);

//	unknown type of subset handler
	throw(UGFatalError("Unknown SubsetHandler type."));
	return false;
}

///	returns the current dimension of the subset
inline int DimensionOfSubset(const SubsetHandler& sh, int si)
{
	// choose dimension
	if(sh.num<Volume>(si) > 0) return 3;
	if(sh.num<Face>(si) > 0) return 2;
	if(sh.num<EdgeBase>(si) > 0) return 1;
	if(sh.num<VertexBase>(si) > 0) return 0;
	else return -1;
}

///	returns the current dimension of the subset
inline int DimensionOfSubset(const MGSubsetHandler& sh, int si)
{
	// choose dimension
	if(sh.num<Volume>(si) > 0) return 3;
	if(sh.num<Face>(si) > 0) return 2;
	if(sh.num<EdgeBase>(si) > 0) return 1;
	if(sh.num<VertexBase>(si) > 0) return 0;
	else return -1;
}

///	returns the current dimension of the subset
inline int DimensionOfSubset(const ISubsetHandler& ish, int si)
{
//	test SubsetHandler
	const SubsetHandler* sh = dynamic_cast<const SubsetHandler*>(&ish);
	if(sh != NULL)
		return DimensionOfSubset(*sh, si);

//	test MGSubsetHandler
	const MGSubsetHandler* mgsh = dynamic_cast<const MGSubsetHandler*>(&ish);
	if(mgsh != NULL)
		return DimensionOfSubset(*mgsh, si);

//	unknown type of subset handler
	return -1;
}

inline int DimensionOfSubsets(const ISubsetHandler& sh)
{
//	dimension to be computed
	int dim = -1;

//	loop subsets
	for(int si = 0; si < sh.num_subsets(); ++si)
	{
	//	get dimension of subset
		int siDim = DimensionOfSubset(sh, si);

	//	if no dimension available, return -1
		if(siDim == -1) return -1;

	//	check if dimension is higher than already checked subsets
		if(dim < siDim)
			dim = siDim;
	}
	return dim;
}

///	returns the current dimension of the subset
template <typename TDomain>
inline int DimensionOfSubset(const TDomain& domain, int si)
{
	// extract subset handler
	const typename TDomain::subset_handler_type& sh = domain.get_subset_handler();

	return DimensionOfSubset(sh, si);
}

//	returns the corner coordinates of a geometric object
template <typename TElem, typename TAAPos>
void CollectCornerCoordinates(	std::vector<typename TAAPos::ValueType>& vCornerCoordsOut,
								const TElem& elem, const TAAPos& aaPos, bool clearContainer)
{
	if(clearContainer)
		vCornerCoordsOut.clear();

	// number of vertices of element
	const size_t numVertices = elem.num_vertices();

	// loop vertices
	for(size_t i = 0; i < numVertices; ++i)
	{
		// get element
		VertexBase* vert = elem.vertex(i);

		// write corner coordinates
		vCornerCoordsOut.push_back(aaPos[vert]);
	}
}

///	returns the corner coordinates of a geometric object
template <typename TElem, typename TDomain>
void CollectCornerCoordinates(	std::vector<typename TDomain::position_type>& vCornerCoordsOut,
								const TElem& elem, const TDomain& domain, bool clearContainer)
{
	// get position accessor
	const typename TDomain::position_accessor_type& aaPos = domain.get_position_accessor();

	CollectCornerCoordinates(vCornerCoordsOut, elem, aaPos, clearContainer);
}

////////////////////////////////////////////////////////////////////////
///	returns the size of a geometric object
template <typename TElem, typename TPosition>
number ElementSize(const TElem& elem, const Grid::VertexAttachmentAccessor<Attachment<TPosition> >& aaPos)
{
	// corner coords
	std::vector<TPosition> vCornerCoords;

	// load corner coords
	CollectCornerCoordinates(vCornerCoords, elem, aaPos);

	// get reference element type
	typedef typename reference_element_traits<TElem>::reference_element_type TRefElem;

	// dimension of Positions
	static const int dim = TPosition::Size;

	// return Element Size
	return ElementSize<TRefElem, dim>(&vCornerCoords[0]);
}

///	returns the size of a geometric object
template <typename TElem, typename TDomain>
number ElementSize(const TElem& elem, const TDomain& domain)
{
	// get position accessor
	const typename TDomain::position_accessor_type& aaPos = domain.get_position_accessor();

	return ElementSize(elem, aaPos);
}

// writes domain to *.ugx file
template <typename TDomain>
bool WriteDomainToUGX(const char* filename, const TDomain& domain)
{
	// filename
	std::string strName = filename;

	// check filename
	if(strName.find(" ") != std::string::npos)
		{UG_LOG("Filename must not include spaces. Cannot write domain."); return false;}

	// check if filename has already ending (if not add it)
	if(strName.find(".ugx") == std::string::npos)
	{
		if(strName.find(".") != std::string::npos)
		{
			UG_LOG("Filename must not include dots. Cannot write domain.");
			return false;
		}
		else
		{
			strName = strName + ".ugx";
		}
	}

	// types
	typedef typename TDomain::grid_type GridType;
	typedef typename TDomain::subset_handler_type SubsetHandlerType ;

	// extract grid and subset handler
	GridType& grid = *const_cast<GridType*>(&domain.get_grid());
	SubsetHandlerType& sh = *const_cast<SubsetHandlerType*>(&domain.get_subset_handler());

	// save grid
	if(!SaveGridToUGX(grid, sh, strName.c_str()))
		{UG_LOG("WriteDomainToUGX: Cannot save grid.\n"); return false;}

	return true;
}

} // end namespace ug

#endif /* __H__LIB_DISCRETIZATION__DOMAIN_UTIL_GENERAL_IMPL__ */
