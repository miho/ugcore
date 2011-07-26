// created by Sebastian Reiter
// s.b.reiter@googlemail.com
// 11.01.2011

#ifndef __H__UG__HANGING_NODE_REFINER_BASE__
#define __H__UG__HANGING_NODE_REFINER_BASE__

#include <queue>
#include "lib_grid/lg_base.h"
#include "refinement_callbacks.h"
#include "refiner_interface.h"

namespace ug
{

///	\addtogroup lib_grid_algorithms_refinement
///	@{

///	Base class for a hanging-node refiner.
/**	A hanging node refiner allows to adaptively refine grid elements
 * by inserting hanging nodes at points where T-junctions would occur
 * (ug::HangingVertex).
 * If further refinement is performed, the refiner automatically
 * takes care to adjust the refined area, so that no more than
 * 1 hanging vertex resides on any edge.
 *
 * Each edge on which a hanging node lies is replaced by a
 * ug::ConstrainingEdge. Child-edges of a constraining edge have the
 * type ug::ConstrainedEdge.
 *
 * If volumes exist, the refiner may also create elements of type
 * ug::ConstrainingTriangle or ug::ConstrainingQuadrilateral, as well
 * as ug::ConstrainedTriangle and ug::ConstrainedQuadrilateral.
 *
 * Use the mark methods to mark elements which
 * shall be refined. A call to refine will then perform the refinement
 * and clear all marks.
 *
 * Please note: If you're using a hanging node refiner, you have to
 * be careful to not destroy the connections between hanging-vertices,
 * and constrained / constraining objects.
 *
 * Specializations of this class exist to support hanging node refinement
 * on flat and on hierarchical grids.
 *
 * Note that you may set a refinement callback, which will be
 * responsible to calculate new positions of newly created vertices.
 * By default a linear refinement callback is created for one of
 * the standard position attachments (ug::aPosition, ug::aPosition2,
 * ug::aPosition1 - whichever is present).
 *
 * This class can't be instantiated directly. Use one of its
 * specializations instead.
 *
 * For Developers: Note that HangingNodeRefinerBase stores flags together with
 * the refinement marks. Precisely the value 128 is used to flag whether an
 * element has to be refined using hanging-node-rules.
 *
 * \sa ug::HangingNodeRefiner_Grid, ug::HangingNodeRefiner_MultiGrid
 */
class HangingNodeRefinerBase : public IRefiner, public GridObserver
{
	public:
		using IRefiner::mark;

	public:
		HangingNodeRefinerBase(IRefinementCallback* refCallback = NULL);
		virtual ~HangingNodeRefinerBase();

		virtual void grid_to_be_destroyed(Grid* grid);

	///	enables or disables node-dependency-order-1.
	/**	\{
	 * If enabled, hanging nodes may only depend on non-hanging nodes.
	 * An edge containing a hanging node thus will not have a hanging-node
	 * as a corner vertex.
	 *
	 * Enabled by default.*/
		void enable_node_dependency_order_1(bool bEnable)	{m_nodeDependencyOrder1 = bEnable;}
		bool node_dependency_order_1_enabled()				{return m_nodeDependencyOrder1;}
	/**	\} */

	///	tells whether higher dimensional neighbors of an object are automatically refined, too.
	/**	This is disabled by default. However - in some applications it may be
	 * desirable.
	 * \{ */
		inline void enable_automark_objects_of_higher_dim(bool enable)	{m_automarkHigherDimensionalObjects = enable;}
		inline bool automark_objects_of_higher_dim_enabled()			{return m_automarkHigherDimensionalObjects;}
	/**	\} */

		virtual void clear_marks();

	///	Marks a element for refinement.
	/**	\{ */
		virtual void mark(VertexBase* v, RefinementMark refMark = RM_REGULAR);
		virtual void mark(EdgeBase* e, RefinementMark refMark = RM_REGULAR);
		virtual void mark(Face* f, RefinementMark refMark = RM_REGULAR);
		virtual void mark(Volume* v, RefinementMark refMark = RM_REGULAR);
	/**	\} */

	///	Returns the mark of a given element.
	/**	\{ */
		virtual RefinementMark get_mark(VertexBase* v);
		virtual RefinementMark get_mark(EdgeBase* e);
		virtual RefinementMark get_mark(Face* f);
		virtual RefinementMark get_mark(Volume* v);
	/**	\} */


	///	performs refinement on the marked elements.
	/**
	 * automatically extends the refinement to avoid multiple hanging nodes
	 * on a single edge or face.
	 *
	 * refine calls several virtual methods, which allow to influence the
	 * refinement process. Most notably the methods
	 *
	 *		- collect_objects_for_refine
	 *		- pre_refine
	 * 		- post_refine
	 *
	 * are called in the given order. During element refinement further
	 * virtual methods are called, which perform the actual element refinement.
	 */
		void refine();

	protected:
	/**	additional mark to RefinementMarks. Used to flag whether an element
	 * will be refined with constraining.*/
		enum HNodeRefMarks{
			HNRM_REFINE_CONSTRAINED = 128
		};

	///	returns true if an element is marked for hnode refinement.
		template<class TElem>
		bool marked_for_hnode_refinement(TElem* elem)
			{return m_selMarkedElements.get_selection_status(elem) & HNRM_REFINE_CONSTRAINED;}

	///	use this method to set whether an element should be refined with hnode refinement.
		template<class TElem>
		void mark_for_hnode_refinement(TElem* elem, bool bMark)
			{
				if(bMark)
					m_selMarkedElements.select(elem,
								m_selMarkedElements.get_selection_status(elem)
									| HNRM_REFINE_CONSTRAINED);
				else
					m_selMarkedElements.select(elem,
								m_selMarkedElements.get_selection_status(elem)
									& ~HNRM_REFINE_CONSTRAINED);
			}

	///	a callback that allows to deny refinement of special vertices
		virtual bool refinement_is_allowed(VertexBase* elem)	{return true;}
	///	a callback that allows to deny refinement of special edges
		virtual bool refinement_is_allowed(EdgeBase* elem)		{return true;}
	///	a callback that allows to deny refinement of special faces
		virtual bool refinement_is_allowed(Face* elem)			{return true;}
	///	a callback that allows to deny refinement of special volumes
		virtual bool refinement_is_allowed(Volume* elem)		{return true;}

	///	performs registration and deregistration at a grid.
	/**	Sets a grid and performs registration at the given grid.
	 * 	The associated selector is also initialised with the given grid.
	 * 	It is cruical to call this method or everything will fail.
	 *
	 *  call set_grid(NULL) to unregister the observer from a grid.
	 *
	 *  Please note that this method is not declared virtual, since it
	 *  is called during construction and destruction.*/
		void set_grid(Grid* grid);

	///	marks unmarked elements that have to be refined due to marked neighbors.
	/**
	 * all elements that have to be refined will be written to the passed queues.
	 * Note that this will most likely be more elements than just the marked ones.
	 *
	 * This method is virtual to allow derivates to mark additional elements as required.
	 * Normally a a derived class will first call the method of its this class and
	 * the perform its own operations.
	 */
		virtual void collect_objects_for_refine();

	/**	This callback is called during execution of the refine() method after
	 * collect_objects_for_refine has returned. It is responsible to mark
	 * elements for hnode refinement. That means all elements on which a hanging
	 * node or constrained children shall be created have to be marked using
	 * mark_for_hnode_refinement during this method. The default implementation
	 * performs this marking for all local elements.
	 * Make sure to not mark any new elements during this method. Marks may only
	 * be adjusted!*/
		virtual void assign_hnode_marks();

	/**	called by refine after collect_objects_for_refine and before
	 *	actual refinement begins.*/
		virtual void pre_refine()	{}

	/**	called by refine after refinement is done.*/
		virtual void post_refine()	{}

	////////////////////////////////////////////////////////////////////////
	//	refine methods
	///	called to refine the specified element.
	/**	Refines the element. Corner vertices of the newly created element
	 *  can be specified through newCornerVrts. newCornerVrts = NULL (default)
	 *  means, that the corner vertices of the original element shall be taken.
	 *  \{
	 */
		virtual void refine_constraining_edge(ConstrainingEdge* cge);
		virtual void refine_edge_with_normal_vertex(EdgeBase* e,
											VertexBase** newCornerVrts = NULL);
		virtual void refine_edge_with_hanging_vertex(EdgeBase* e,
											VertexBase** newCornerVrts = NULL);

		virtual void refine_face_with_normal_vertex(Face* f,
											VertexBase** newCornerVrts = NULL);
		virtual void refine_face_with_hanging_vertex(Face* f,
											VertexBase** newCornerVrts = NULL);
		virtual void refine_constraining_face(ConstrainingFace* cgf);

		virtual void refine_volume_with_normal_vertex(Volume* v,
											VertexBase** newVolumeVrts = NULL);
	/**	\} */

	////////////////////////////////////////////////////////////////////////
	//	helpers. Make sure that everything is initialized properly
	//	before calling these methods.
	//	you should use this methods instead of directly marking elements.
		inline bool is_marked(VertexBase* v)				{return m_selMarkedElements.is_selected(v);}
		//inline void mark(VertexBase* v)						{mark(v);}

		inline bool is_marked(EdgeBase* e)					{return m_selMarkedElements.is_selected(e);}
		//inline void mark(EdgeBase* e)						{mark(e);}

	///	Returns the vertex associated with the edge
	/**	pure virtual method.
	 *	Has to return the center vertex which was set to the edge via
	 *	set_center_vertex. If no vertex was set, NULL has to be returned.*/
		virtual VertexBase* get_center_vertex(EdgeBase* e) = 0;

	///	Associates a vertex with the edge (pure virtual).
		virtual void set_center_vertex(EdgeBase* e, VertexBase* v) = 0;

		inline bool is_marked(Face* f)						{return m_selMarkedElements.is_selected(f);}
		//inline void mark(Face* f)							{mark(f);}

	///	Returns the vertex associated with the face
	/**	pure virtual method.
	 *	Has to return the center vertex which was set to the face via
	 *	set_center_vertex. If no vertex was set, NULL has to be returned.*/
		virtual VertexBase* get_center_vertex(Face* f) = 0;

	///	Associates a vertex with the face (pure virtual).
		virtual void set_center_vertex(Face* f, VertexBase* v) = 0;

		inline bool is_marked(Volume* v)					{return m_selMarkedElements.is_selected(v);}
		//inline void mark(Volume* v)						{mark(v);}

		template <class TElem>
		inline bool marked_regular(TElem* elem)				{return m_selMarkedElements.get_selection_status(elem) == RM_REGULAR;}

		template <class TElem>
		inline bool marked_anisotropic(TElem* elem)			{return m_selMarkedElements.get_selection_status(elem) == RM_ANISOTROPIC;}

		template <class TElem>
		inline bool marked_coarsen(TElem* elem)			{return m_selMarkedElements.get_selection_status(elem) == RM_COARSEN;}

	/**	used during collect_objects_for_refine.
	 *	unmarked associated elements of the elements between elemsBegin and
	 *	elemsEnd are pushed to the queue.
	 *	Please note that a single object may appear multiple times in the queue.
	 *	\{ */
		template <class TIterator>
		void collect_associated_unmarked_edges(
							std::queue<EdgeBase*>& qEdgesOut, Grid& grid,
							TIterator elemsBegin, TIterator elemsEnd,
						 	bool ignoreAnisotropicElements);

		template <class TIterator>
		void collect_associated_unmarked_faces(
							std::queue<Face*>& qFacesOut, Grid& grid,
							TIterator elemsBegin, TIterator elemsEnd,
							bool ignoreAnisotropicElements);

		template <class TIterator>
		void collect_associated_unmarked_volumes(
							std::queue<Volume*>& qVolsOut, Grid& grid,
							TIterator elemsBegin, TIterator elemsEnd,
							bool ignoreAnisotropicElements);
	/** \} */

		///	returns the selector which is internally used to mark elements.
		/**	Be sure to use it carefully!*/
		Selector& get_refmark_selector()	{return m_selMarkedElements;}

	private:
	///	private copy constructor to avoid copy construction
		HangingNodeRefinerBase(const HangingNodeRefinerBase&);

	protected:
		Selector	m_selMarkedElements;

	private:
		Grid*		m_pGrid;
		bool		m_nodeDependencyOrder1;
		bool		m_automarkHigherDimensionalObjects;
};

/// @}	// end of add_to_group command

}//	end of namespace

#endif
