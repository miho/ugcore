/*
 * linear_elasticity.h
 *
 *  Created on: 05.08.2010
 *      Author: andreasvogel
 */

#ifndef __H__UG__LIB_DISC__SPATIAL_DISC__ELEM_DISC__LINEAR_ELASTICITY__FE1_LINEAR_ELASTICITY__
#define __H__UG__LIB_DISC__SPATIAL_DISC__ELEM_DISC__LINEAR_ELASTICITY__FE1_LINEAR_ELASTICITY__

// other ug4 modules
#include "common/common.h"
#include "lib_grid/lg_base.h"

// library intern headers
#include "lib_disc/spatial_disc/elem_disc/elem_disc_interface.h"

namespace ug{


template<typename TDomain>
class FE1LinearElasticityElemDisc
	: public IDomainElemDisc<TDomain>
{
	private:
	///	Base class type
		typedef IDomainElemDisc<TDomain> base_type;

	///	own type
		typedef FE1LinearElasticityElemDisc<TDomain> this_type;

	public:
	///	Domain type
		typedef typename base_type::domain_type domain_type;

	///	World dimension
		static const int dim = base_type::dim;

	///	Position type
		typedef typename base_type::position_type position_type;

	protected:
		typedef void (*Elasticity_Tensor_fct)(MathTensor<4,dim>&);

	public:
		FE1LinearElasticityElemDisc(Elasticity_Tensor_fct elast);

		virtual size_t num_fct(){return dim;}

	///	type of trial space for each function used
		virtual bool request_finite_element_id(const std::vector<LFEID>& vLfeID)
		{
		//	check number
			if(vLfeID.size() != num_fct()) return false;

		//	check that Lagrange 1st order
			for(size_t i = 0; i < vLfeID.size(); ++i)
				if(vLfeID[i] != LFEID(LFEID::LAGRANGE, 1)) return false;
			return true;
		}

	///	switches between non-regular and regular grids
		virtual bool treat_non_regular_grid(bool bNonRegular)
		{
		//	no special care for non-regular grids
			return true;
		}

	private:
		template <typename TElem>
		bool prepare_element_loop();

		template <typename TElem>
		bool prepare_element(TElem* elem, const LocalVector& u);

		template <typename TElem>
		bool finish_element_loop();

		template <typename TElem>
		bool assemble_JA(LocalMatrix& J, const LocalVector& u);

		template <typename TElem>
		bool assemble_JM(LocalMatrix& J, const LocalVector& u);

		template <typename TElem>
		bool assemble_A(LocalVector& d, const LocalVector& u);

		template <typename TElem>
		bool assemble_M(LocalVector& d, const LocalVector& u);

		template <typename TElem>
		bool assemble_f(LocalVector& d);

	private:
		// position access
		const position_type* m_corners;

		// User functions
		Elasticity_Tensor_fct m_ElasticityTensorFct;
		MathTensor<4, dim> m_ElasticityTensor;

	private:
		void register_all_fe1_funcs();

		struct RegisterFE1 {
				RegisterFE1(this_type* pThis) : m_pThis(pThis){}
				this_type* m_pThis;
				template< typename TElem > void operator()(TElem&)
				{m_pThis->register_fe1_func<TElem>();}
		};

		template <typename TElem>
		void register_fe1_func();
};

}

#endif /*__H__UG__LIB_DISC__SPATIAL_DISC__ELEM_DISC__LINEAR_ELASTICITY__FE1_LINEAR_ELASTICITY__*/
