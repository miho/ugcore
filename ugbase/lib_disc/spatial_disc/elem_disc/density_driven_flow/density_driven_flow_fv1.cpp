/*
 * density_driven_flow_fv1.cpp
 *
 *  Created on: 26.02.2010
 *      Author: andreasvogel
 */

#include "density_driven_flow.h"
#include "common/util/provider.h"
#include "lib_disc/spatial_disc/ip_data/const_user_data.h"

//#define PROFILE_D3F
#ifdef PROFILE_D3F
	#define D3F_PROFILE_FUNC()		PROFILE_FUNC()
	#define D3F_PROFILE_BEGIN(name)	PROFILE_BEGIN(name)
	#define D3F_PROFILE_END()		PROFILE_END()
#else
	#define D3F_PROFILE_FUNC()
	#define D3F_PROFILE_BEGIN(name)
	#define D3F_PROFILE_END()
#endif

namespace ug{

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//  Provide a generic implementation for all elements
//  (since this discretization can be implemented in a generic way)
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

template<typename TDomain>
void
DensityDrivenFlowElemDisc<TDomain>::
compute_darcy_velocity_ip_std(MathVector<dim>& DarcyVel,
                              const MathMatrix<dim, dim>& Permeability,
                              number Viscosity,
                              const MathVector<dim>& DensityTimesGravity,
                              const MathVector<dim>& PressureGrad,
                              bool compDeriv,
                              MathVector<dim>* DarcyVel_c,
                              MathVector<dim>* DarcyVel_p,
                              const MathVector<dim>* DensityTimesGravity_c,
                              const number* Viscosity_c,
                              const MathVector<dim>* PressureGrad_p,
                              size_t numSh)
{
//	Variables
	MathVector<dim> Vel;

//	compute inverse viscocity
	const number InvVisco = 1./Viscosity;

// 	compute rho*g - \nabla p
	VecSubtract(Vel, DensityTimesGravity, PressureGrad);

//	compute Darcy velocity q := K / mu * (rho*g - \nabla p)
	MatVecMult(DarcyVel, Permeability, Vel);
	VecScale(DarcyVel, DarcyVel, InvVisco);

//	if no derivative needed, done for this point
	if(!compDeriv) return;

//	check correct data
	UG_ASSERT(DarcyVel_c != NULL, "zero pointer");
	UG_ASSERT(DarcyVel_p != NULL, "zero pointer");

//	Derivative of Darcy Vel w.r.t. pressure
	for(size_t sh = 0; sh < numSh; ++sh)
	{
	//	scale by permeability
		MatVecMult(DarcyVel_p[sh], Permeability, PressureGrad_p[sh]);

	//	scale by viscosity
		VecScale(DarcyVel_p[sh], DarcyVel_p[sh], -InvVisco);
	}

//	Derivative of Vel w.r.t. brine mass fraction
	for(size_t sh = 0; sh < numSh; ++sh)
	{
	//	scale by permeability
		MatVecMult(DarcyVel_c[sh], Permeability, DensityTimesGravity_c[sh]);

	//	scale by viscosity
		VecScale(DarcyVel_c[sh], DarcyVel_c[sh], InvVisco);
	}

//	Viscosity derivative
	if(Viscosity_c != NULL)
		for(size_t sh = 0; sh < numSh; ++sh)
		{
		//  DarcyVel_c[sh] -= mu_c_sh(c) / mu * q
			VecSubtract(DarcyVel_c[sh], DarcyVel, - Viscosity_c[sh] * InvVisco);
		}
}


template<typename TDomain>
template <typename TElem>
bool
DensityDrivenFlowElemDisc<TDomain>::
ex_darcy_std(const LocalVector& u,
             const MathVector<dim> vGlobIP[],
             const MathVector<FV1Geometry<TElem,dim>::dim> vLocIP[],
             const size_t nip,
             MathVector<dim> vValue[],
             bool bDeriv,
             std::vector<std::vector<MathVector<dim> > > vvvDeriv[])
{
// 	Get finite volume geometry
	static const FV1Geometry<TElem, dim>& geo =	Provider<FV1Geometry<TElem,dim> >::get();

//	Constants
	static const size_t numSh = FV1Geometry<TElem, dim>::numSCV;

//	FV1 SCVF ip
	if(vLocIP == geo.scvf_local_ips())
	{
	//	Loop ips
		for(size_t ip = 0; ip < nip; ++ip)
		{
		//	Darcy Velocity to be filled
			MathVector<dim>& DarcyVel = vValue[ip];

		//	Derivatives to be filled
			MathVector<dim>* DarcyVel_c = NULL;
			MathVector<dim>* DarcyVel_p = NULL;

			const number* Viscosity_c = NULL;
			const number* Density_c = NULL;
			const MathVector<dim>* PressureGrad_p = NULL;

			MathVector<dim> DensityTimesGravity;
			MathVector<dim> DensityTimesGravity_c[numSh];

		//	get data fields to fill
			if(bDeriv)
			{
				DarcyVel_c = &vvvDeriv[ip][_C_][0];
				DarcyVel_p = &vvvDeriv[ip][_P_][0];

			//	get derivative of viscosity w.r.t mass fraction
				if(!m_imViscosityScvf.constant_data())
					Viscosity_c = m_imViscosityScvf.deriv(ip, _C_);

			//	compute rho_c_sh * g
				if(!m_imDensityScvf.constant_data())
				{
					Density_c = m_imDensityScvf.deriv(ip, _C_);
					for(size_t sh = 0; sh < numSh; ++sh)
						VecScale(DensityTimesGravity_c[sh], m_Gravity, Density_c[sh]);
				}

			//	get derivative of gradient derivative
				PressureGrad_p = m_imPressureGradScvf.deriv(ip, _P_);
			}

		//	Compute DensityTimesGravity = rho * g
			VecScale(DensityTimesGravity, m_Gravity, m_imDensityScvf[ip]);

		//	compute Darcy velocity (and derivative if required)
			compute_darcy_velocity_ip_std(DarcyVel, m_imPermeabilityScvf[ip],
			                              m_imViscosityScvf[ip], DensityTimesGravity,
			                              m_imPressureGradScvf[ip],
			                              bDeriv,
			                              DarcyVel_c, DarcyVel_p, DensityTimesGravity_c,
			                              Viscosity_c, PressureGrad_p, numSh);
		}
	}
// 	others not implemented
	else
	{
		UG_LOG("Evaluation not implemented.");
		return false;
	}

//	we're done
	return true;
}


template<typename TDomain>
template <typename TElem>
bool
DensityDrivenFlowElemDisc<TDomain>::
ex_darcy_cons_grav(const LocalVector& u,
                   const MathVector<dim> vGlobIP[],
                   const MathVector<FV1Geometry<TElem,dim>::dim> vLocIP[],
                   const size_t nip,
                   MathVector<dim> vValue[],
                   bool bDeriv,
                   std::vector<std::vector<MathVector<dim> > > vvvDeriv[])
{
// 	Get finite volume geometry
	static const FV1Geometry<TElem, dim>& geo =	Provider<FV1Geometry<TElem,dim> >::get();

//	Constants
	static const size_t numSh = FV1Geometry<TElem, dim>::numSCV;

/// Consistent gravity and its derivative at corners
	MathVector<dim> vConsGravity[dim*dim];
	MathVector<dim> vvDConsGravity[dim*dim][dim*dim];

// 	Prepare Density in Corners
	if(!PrepareConsistentGravity<dim>(
			&vConsGravity[0], numSh, &m_vCornerCoords[0], m_imDensityScv.values(),m_Gravity))
	{
		UG_LOG("ERROR in assemble_JA: Cannot prepare Consistent Gravity.\n");
		return false;
	}

// 	Prepare DensityDerivative in Corners
	if(bDeriv) {
		number DCoVal[numSh]; memset(DCoVal, 0, sizeof(number)*numSh);
		for(size_t sh = 0; sh < numSh; sh++) {
			DCoVal[sh] = m_imDensityScv.deriv(sh, _C_, sh);
			if(!PrepareConsistentGravity<dim>(
					&vvDConsGravity[sh][0], numSh, &m_vCornerCoords[0],
					&DCoVal[0], m_Gravity))
			{
				UG_LOG("ERROR in assemble_JA: Cannot prepare Consistent Gravity.\n");
				return false;
			}
			DCoVal[sh] = 0.0;
		}
	}

//	FV1 SCVF ip
	if(vLocIP == geo.scvf_local_ips())
	{
	//	Loop Sub Control Volume Faces (SCVF)
		for(size_t ip = 0; ip < geo.num_scvf(); ++ip)
		{
		// 	Get current SCVF
			const typename FV1Geometry<TElem, dim>::SCVF& scvf = geo.scvf(ip);

		//	Darcy Velocity to be filled
			MathVector<dim>& DarcyVel = vValue[ip];

		//	Derivatives to be filled
			MathVector<dim>* DarcyVel_c = NULL;
			MathVector<dim>* DarcyVel_p = NULL;

			const number* Viscosity_c = NULL;
			const MathVector<dim>* PressureGrad_p = NULL;

			MathVector<dim> DensityTimesGravity;
			MathVector<dim> DensityTimesGravity_c[numSh];

		//	get data fields to fill
			if(bDeriv)
			{
				DarcyVel_c = &vvvDeriv[ip][_C_][0];
				DarcyVel_p = &vvvDeriv[ip][_P_][0];

			//	get derivative of viscosity w.r.t mass fraction
				if(!m_imViscosityScvf.constant_data())
					Viscosity_c = m_imViscosityScvf.deriv(ip, _C_);

			//	compute rho_c_sh * g
				for(size_t sh = 0; sh < numSh; ++sh)
					if(!ComputeConsistentGravity<dim>(
							DensityTimesGravity_c[sh], numSh, scvf.JTInv(),
							scvf.local_grad_vector(), &vvDConsGravity[sh][0]))
					{
						UG_LOG("ERROR in compute_ip_Darcy_velocity: Cannot "
								"Compute Consistent Gravity.\n");
						return false;
					}

			//	get derivative of gradient derivative
				PressureGrad_p = m_imPressureGradScvf.deriv(ip, _P_);
			}

		//	Compute DensityTimesGravity = rho * g
			if(!ComputeConsistentGravity<dim>(
					DensityTimesGravity, numSh, scvf.JTInv(),
					scvf.local_grad_vector(), vConsGravity))
			{
				UG_LOG("ERROR in compute_ip_Darcy_velocity: Cannot "
						"Compute Consistent Gravity.\n");
				return false;
			}

		//	compute Darcy velocity (and derivative if required)
			compute_darcy_velocity_ip_std(DarcyVel, m_imPermeabilityScvf[ip],
										  m_imViscosityScvf[ip], DensityTimesGravity,
										  m_imPressureGradScvf[ip],
										  bDeriv,
										  DarcyVel_c, DarcyVel_p, DensityTimesGravity_c,
										  Viscosity_c, PressureGrad_p, numSh);
		}
	}
// 	others not implemented
	else
	{
		UG_LOG("Evaluation not implemented.");
		return false;
	}

//	we're done
	return true;
}

template<typename TDomain>
template<typename TElem >
bool
DensityDrivenFlowElemDisc<TDomain>::
ex_brine(const LocalVector& u,
         const MathVector<dim> vGlobIP[],
         const MathVector<FV1Geometry<TElem,dim>::dim> vLocIP[],
         const size_t nip,
         number vValue[],
         bool bDeriv,
         std::vector<std::vector<number> > vvvDeriv[])
{
// 	Get finite volume geometry
	static const FV1Geometry<TElem, dim>& geo =	Provider<FV1Geometry<TElem,dim> >::get();

//	Constants
	static const size_t numSh = FV1Geometry<TElem, dim>::numSCV;

//	FV1 SCVF ip
	if(vLocIP == geo.scvf_local_ips())
	{
	//	Loop Sub Control Volume Faces (SCVF)
		for(size_t ip = 0; ip < geo.num_scvf(); ++ip)
		{
		// 	Get current SCVF
			const typename FV1Geometry<TElem, dim>::SCVF& scvf = geo.scvf(ip);

		//	Compute Gradients and concentration at ip
			vValue[ip] = 0.0;
			for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
				vValue[ip] += u(_C_, sh) * scvf.shape(sh);

		//	compute derivative w.r.t. to unknowns iff needed
			if(bDeriv)
				for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
				{
					vvvDeriv[ip][_C_][sh] = scvf.shape(sh);
					vvvDeriv[ip][_P_][sh] = 0.0;
				}
		}
	}
	//	FV1 SCV ip
	else if(vLocIP == geo.scv_local_ips())
	{
	//	solution at ip
		for(size_t ip = 0; ip < numSh; ++ip)
			vValue[ip] = u(_C_, ip);

	//	set derivatives if needed
		if(bDeriv)
			for(size_t ip = 0; ip < numSh; ++ip)
				for(size_t sh = 0; sh < numSh; ++sh)
				{
					vvvDeriv[ip][_C_][sh] = (ip==sh) ? 1.0 : 0.0;
					vvvDeriv[ip][_P_][sh] = 0.0;
				}
	}
// 	others not implemented
	else
	{
		UG_LOG("Evaluation not implemented.");
		return false;
	}

//	we're done
	return true;
}

template<typename TDomain>
template<typename TElem >
bool
DensityDrivenFlowElemDisc<TDomain>::
ex_brine_grad(const LocalVector& u,
              const MathVector<dim> vGlobIP[],
              const MathVector<FV1Geometry<TElem,dim>::dim> vLocIP[],
              const size_t nip,
              MathVector<dim> vValue[],
              bool bDeriv,
              std::vector<std::vector<MathVector<dim> > > vvvDeriv[])
{
// 	Get finite volume geometry
	static const FV1Geometry<TElem, dim>& geo =	Provider<FV1Geometry<TElem,dim> >::get();

//	FV1 SCVF ip
	if(vLocIP == geo.scvf_local_ips())
	{
	//	Loop Sub Control Volume Faces (SCVF)
		for(size_t ip = 0; ip < geo.num_scvf(); ++ip)
		{
		// 	Get current SCVF
			const typename FV1Geometry<TElem,dim>::SCVF& scvf = geo.scvf(ip);

			VecSet(vValue[ip], 0.0);
			for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
				VecScaleAppend(vValue[ip], u(_C_, sh), scvf.global_grad(sh));

			if(bDeriv)
				for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
				{
					vvvDeriv[ip][_C_][sh] = scvf.global_grad(sh);
					vvvDeriv[ip][_P_][sh] = 0.0;
				}
		}
	}
// others not implemented
	else
	{
		UG_LOG("Evaluation not implemented.");
		return false;
	}

//	we're done
	return true;
}

template<typename TDomain>
template<typename TElem >
bool
DensityDrivenFlowElemDisc<TDomain>::
ex_pressure_grad(const LocalVector& u,
                 const MathVector<dim> vGlobIP[],
                 const MathVector<FV1Geometry<TElem,dim>::dim> vLocIP[],
                 const size_t nip,
                 MathVector<dim> vValue[],
                 bool bDeriv,
                 std::vector<std::vector<MathVector<dim> > > vvvDeriv[])
{
// 	Get finite volume geometry
	static const FV1Geometry<TElem, dim>& geo =	Provider<FV1Geometry<TElem,dim> >::get();

//	FV1 SCVF ip
	if(vLocIP == geo.scvf_local_ips())
	{
	//	Loop Sub Control Volume Faces (SCVF)
		for(size_t ip = 0; ip < geo.num_scvf(); ++ip)
		{
		// 	Get current SCVF
			const typename FV1Geometry<TElem,dim>::SCVF& scvf = geo.scvf(ip);

			VecSet(vValue[ip], 0.0);
			for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
				VecScaleAppend(vValue[ip], u(_P_, sh), scvf.global_grad(sh));

			if(bDeriv)
				for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
				{
					vvvDeriv[ip][_C_][sh] = 0.0;
					vvvDeriv[ip][_P_][sh] = scvf.global_grad(sh);
				}
		}
	}
// others not implemented
	else
	{
		UG_LOG("Evaluation not implemented.");
		return false;
	}

//	we're done
	return true;
}

template<typename TDomain>
template<typename TElem >
bool
DensityDrivenFlowElemDisc<TDomain>::
prepare_element_loop()
{
// 	Resizes
	typedef typename reference_element_traits<TElem>::reference_element_type
					ref_elem_type;

//	reference dimension
	static const int refDim = ref_elem_type::dim;

//	check, that upwind has been set
	if(m_pUpwind == NULL)
	{
		UG_LOG("ERROR in 'DensityDrivenFlowElemDisc::prepare_element_loop':"
				" Upwind has not been set.\n");
		return false;
	}

//	init upwind for element type
	if(!m_pUpwind->template set_geometry_type<FV1Geometry<TElem, dim> >())
	{
		UG_LOG("ERROR in 'DensityDrivenFlowElemDisc::prepare_element_loop':"
				" Cannot init upwind for element type.\n");
		return false;
	}

//	check necessary imports
	if(!m_imBrineScvf.data_given())
	{
		UG_LOG("DensityDrivenFlowElemDisc::prepare_element_loop:"
				" Missing Import: Brine mass fraction.\n");
		return false;
	}
	if(!m_imBrineGradScvf.data_given())
	{
		UG_LOG("DensityDrivenFlowElemDisc::prepare_element_loop:"
				" Missing Import: Gradient of Brine mass fraction.\n");
		return false;
	}
	if(!m_imPressureGradScvf.data_given())
	{
		UG_LOG("DensityDrivenFlowElemDisc::prepare_element_loop:"
				" Missing Import: Gradient of Pressure.\n");
		return false;
	}
	if(!m_imPorosityScvf.data_given())
	{
		UG_LOG("DensityDrivenFlowElemDisc::prepare_element_loop:"
				" Missing user function: Porosity.\n");
		return false;
	}
	if(!m_imPermeabilityScvf.data_given())
	{
		UG_LOG("DensityDrivenFlowElemDisc::prepare_element_loop:"
				" Missing user function: Permeability.\n");
		return false;
	}
	if(!m_imMolDiffusionScvf.data_given())
	{
		UG_LOG("DensityDrivenFlowElemDisc::prepare_element_loop:"
				" Missing user function: Molecular Diffusion.\n");
		return false;
	}
	if(!m_imViscosityScvf.data_given())
	{
		UG_LOG("DensityDrivenFlowElemDisc::prepare_element_loop:"
				" Missing user function: Viscosity.\n");
		return false;
	}
	if(!m_imDensityScvf.data_given())
	{
		UG_LOG("DensityDrivenFlowElemDisc::prepare_element_loop:"
				" Missing user function: Density.\n");
		return false;
	}
	if(!m_imDarcyVelScvf.data_given())
	{
		UG_LOG("DensityDrivenFlowElemDisc::prepare_element_loop:"
				" Missing Import: Darcy Velocity.\n");
		return false;
	}
	if(!m_imPorosityScv.data_given())
	{
		UG_LOG("DensityDrivenFlowElemDisc::prepare_element_loop:"
				" Missing user function: Porosity.\n");
		return false;
	}
	if(!m_imDensityScv.data_given())
	{
		UG_LOG("DensityDrivenFlowElemDisc::prepare_element_loop:"
				" Missing user function: Density.\n");
		return false;
	}
	if(m_imConstGravity.constant_data())
	{
	//	cast gravity export
		ConstUserVector<dim>* pGrav
			= dynamic_cast<ConstUserVector<dim>*>(m_imConstGravity.get_data());

	//	check success
		if(pGrav == NULL)
		{
			UG_LOG("ERROR in prepare_element_loop: Cannot cast constant gravity.\n");
			return false;
		}

	//	evaluate constant data
		MathVector<dim> dummy;
		pGrav->operator ()(m_Gravity, dummy, 0.0);
	}
	else
	{
		UG_LOG("ERROR in prepare_element_loop: Gravity must be constant.\n");
		return false;
	}


//	set local positions for user data
	FV1Geometry<TElem, dim>& geo = Provider<FV1Geometry<TElem,dim> >::get();

	const MathVector<refDim>* vSCVFip = geo.scvf_local_ips();
	size_t numSCVFip = geo.num_scvf_ips();
	m_imBrineScvf.			template set_local_ips<refDim>(vSCVFip, numSCVFip);
	m_imBrineGradScvf.		template set_local_ips<refDim>(vSCVFip, numSCVFip);
	m_imPressureGradScvf.	template set_local_ips<refDim>(vSCVFip, numSCVFip);
	m_imPorosityScvf.		template set_local_ips<refDim>(vSCVFip, numSCVFip);
	m_imPermeabilityScvf.	template set_local_ips<refDim>(vSCVFip, numSCVFip);
	m_imMolDiffusionScvf.	template set_local_ips<refDim>(vSCVFip, numSCVFip);
	m_imViscosityScvf.		template set_local_ips<refDim>(vSCVFip, numSCVFip);
	m_imDensityScvf.		template set_local_ips<refDim>(vSCVFip, numSCVFip);
	m_imDarcyVelScvf.		template set_local_ips<refDim>(vSCVFip, numSCVFip);

	const MathVector<refDim>* vSCVip = geo.scv_local_ips();
	size_t numSCVip = geo.num_scv_ips();
	m_imPorosityScv.	template set_local_ips<refDim>(vSCVip, numSCVip);
	m_imDensityScv.		template set_local_ips<refDim>(vSCVip, numSCVip);

//	we're done
	return true;
}


template<typename TDomain>
bool
DensityDrivenFlowElemDisc<TDomain>::
time_point_changed(number time)
{
//	set new time point at imports
	m_imBrineScvf.set_time(time);
	m_imBrineGradScvf.set_time(time);
	m_imPressureGradScvf.set_time(time);
	m_imPressureGradScvf.set_time(time);
	m_imPorosityScvf.set_time(time);
	m_imPermeabilityScvf.set_time(time);
	m_imMolDiffusionScvf.set_time(time);
	m_imViscosityScvf.set_time(time);
	m_imDensityScvf.set_time(time);
	m_imDarcyVelScvf.set_time(time);
	m_imPorosityScv.set_time(time);
	m_imDensityScv.set_time(time);

//	this disc does not need the old time solutions, thus, return false
	return false;
}

template<typename TDomain>
template<typename TElem >
bool
DensityDrivenFlowElemDisc<TDomain>::
finish_element_loop()
{
	return true;
}

template<typename TDomain>
template<typename TElem >
bool
DensityDrivenFlowElemDisc<TDomain>::
prepare_element(TElem* elem, const LocalVector& u)
{
//	reference element
	typedef typename reference_element_traits<TElem>::reference_element_type ref_elem_type;

//	get corners
	m_vCornerCoords = this->template get_element_corners<TElem>(elem);

// 	Update Geometry for this element
	static FV1Geometry<TElem, dim>& geo = Provider<FV1Geometry<TElem,dim> >::get();

	if(!geo.update(elem, &m_vCornerCoords[0], &(this->get_subset_handler())))
	{
		UG_LOG("FVConvectionDiffusionElemDisc::prepare_element:"
				" Cannot update Finite Volume Geometry.\n"); return false;
	}

//	set global positions for user data
	const MathVector<dim>* vSCVFip = geo.scvf_global_ips();
	size_t numSCVFip = geo.num_scvf_ips();
	m_imBrineScvf.			set_global_ips(vSCVFip, numSCVFip);
	m_imBrineGradScvf.		set_global_ips(vSCVFip, numSCVFip);
	m_imPressureGradScvf.	set_global_ips(vSCVFip, numSCVFip);
	m_imPorosityScvf.		set_global_ips(vSCVFip, numSCVFip);
	m_imPermeabilityScvf.	set_global_ips(vSCVFip, numSCVFip);
	m_imMolDiffusionScvf.	set_global_ips(vSCVFip, numSCVFip);
	m_imViscosityScvf.		set_global_ips(vSCVFip, numSCVFip);
	m_imDensityScvf.		set_global_ips(vSCVFip, numSCVFip);
	m_imDarcyVelScvf.		set_global_ips(vSCVFip, numSCVFip);

	const MathVector<dim>* vSCVip = geo.scv_global_ips();
	size_t numSCVip = geo.num_scv_ips();
	m_imPorosityScv.		set_global_ips(vSCVip, numSCVip);
	m_imDensityScv.			set_global_ips(vSCVip, numSCVip);

//	we're done
	return true;
}

template<typename TDomain>
template<typename TElem >
bool
DensityDrivenFlowElemDisc<TDomain>::
assemble_JA(LocalMatrix& J, const LocalVector& u)
{
// 	Get finite volume geometry
	static const FV1Geometry<TElem, dim>& geo =	Provider<FV1Geometry<TElem,dim> >::get();

//	static numbers, known at compile-time
	static const size_t numSh = FV1Geometry<TElem, dim>::numSCV;
	static const size_t numScvf = FV1Geometry<TElem, dim>::numSCVF;

// 	Some variables
	MathMatrix<dim,dim> D, Diffusion[numScvf];
	MathVector<dim> Dgrad;
	number vDFlux_c[numSh], vDFlux_p[numSh];

//	compute Diffusion part of the Diffusion-Dispersion Tensor
	for(size_t s = 0; s < numScvf; ++s)
		MatScale(Diffusion[s], m_imPorosityScvf[s], m_imMolDiffusionScvf[s]);

//	compute Dispersion part
	// todo: Compute DarcyVelocity for Dispersion
	// todo: Compute Dispersion

//	compute upwind shapes for transport equation
	if(!m_pUpwind->update(&geo, m_imDarcyVelScvf.values(), Diffusion, true))
	{
		UG_LOG("ERROR in 'DensityDrivenFlowElemDisc::assemble_JA': "
				"Cannot compute convection shapes.\n");
		return false;
	}

//	get a const (!!) reference to the upwind
	const IConvectionShapes<dim>& convShape
		= *const_cast<const IConvectionShapes<dim>*>(m_pUpwind);

//	Loop Sub Control Volume Faces (SCVF)
	for(size_t ip = 0; ip < geo.num_scvf(); ++ip)
	{
	// 	Get current SCVF
		const typename FV1Geometry<TElem, dim>::SCVF& scvf = geo.scvf(ip);

		///////////////////////////////////////////
		// Darcy Velocity at ip
		///////////////////////////////////////////

		const MathVector<dim>* vDDarcyVel_c = m_imDarcyVelScvf.deriv(ip, _C_);
		const MathVector<dim>* vDDarcyVel_p = m_imDarcyVelScvf.deriv(ip, _P_);

		////////////////////////
		// Transport Equation
		////////////////////////

	//	Loop Shape Functions
		for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
		{
		//	Compute Derivative of Convective Flux
			vDFlux_c[sh] = convShape(ip, sh);
			vDFlux_p[sh] = 0.0;

		//	Derivative w.r.t. Velocity
			for(size_t sh1 = 0; sh1 < scvf.num_sh(); ++sh1)
			{
				vDFlux_c[sh] += u(_C_, sh1) * VecDot(convShape.D_vel(ip, sh1), vDDarcyVel_c[sh]);
				vDFlux_p[sh] += u(_C_, sh1) * VecDot(convShape.D_vel(ip, sh1), vDDarcyVel_p[sh]);
			}

			//todo: Derivative of Dispersion

		//	Add Derivative of Diffusive Flux
			MatVecMult(Dgrad, Diffusion[ip], scvf.global_grad(sh));
			vDFlux_c[sh] -= VecDot(Dgrad, scvf.normal());

			// todo: Derivative of Dispersion
		}

	//	Handle density in case of full equation
		if(!m_BoussinesqTransport)
		{
		//	Convective Flux
			number flux = 0;
			for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
				flux += convShape(ip, sh) * u(_C_, sh);

		//	Diffusive Flux
			MatVecMult(Dgrad, Diffusion[ip], m_imBrineGradScvf[ip]);
			flux -= VecDot(Dgrad, scvf.normal());

		//	Derivative of product
			for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
			{
				vDFlux_c[sh] = m_imDensityScvf[ip] * vDFlux_c[sh] +
							   m_imDensityScvf.deriv(ip, _C_, sh) * flux;
				vDFlux_p[sh] *= m_imDensityScvf[ip];
			}
		}

	//	Add Flux contribution
		for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
		{
			J(_C_, scvf.from(), _C_, sh) += vDFlux_c[sh];
			J(_C_, scvf.to(),   _C_, sh) -= vDFlux_c[sh];
			J(_C_, scvf.from(), _P_, sh) += vDFlux_p[sh];
			J(_C_, scvf.to(),   _P_, sh) -= vDFlux_p[sh];
		}

		///////////////////
		// Flow equation
		///////////////////

		for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
		{
			vDFlux_c[sh] = VecDot(vDDarcyVel_c[sh], scvf.normal());
			vDFlux_p[sh] = VecDot(vDDarcyVel_p[sh], scvf.normal());
		}

		if(!m_BoussinesqFlow)
		{
			number flux = VecDot(m_imDarcyVelScvf[ip], scvf.normal());
			for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
			{
				vDFlux_c[sh] = m_imDensityScvf[ip] * vDFlux_c[sh] +
							   m_imDensityScvf.deriv(ip, _C_, sh) * flux;
				vDFlux_p[sh] *= m_imDensityScvf[ip];
			}
		}

	//	Add Flux contribution
		for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
		{
			J(_P_, scvf.from(), _C_, sh) += vDFlux_c[sh];
			J(_P_, scvf.to(),   _C_, sh) -= vDFlux_c[sh];
			J(_P_, scvf.from(), _P_, sh) += vDFlux_p[sh];
			J(_P_, scvf.to(),   _P_, sh) -= vDFlux_p[sh];
		}
	}

//	we're done
	return true;
}

template<typename TDomain>
template<typename TElem >
bool
DensityDrivenFlowElemDisc<TDomain>::
assemble_A(LocalVector& d, const LocalVector& u)
{
//	Get finite volume geometry
	static const FV1Geometry<TElem, dim>& geo =	Provider<FV1Geometry<TElem,dim> >::get();

//	number of subcontrol volume faces
	static const size_t numScvf = FV1Geometry<TElem, dim>::numSCVF;

//	Some variables
	MathMatrix<dim,dim> D, Diffusion[numScvf];
	MathVector<dim> Dgrad_c_ip;

	for(size_t s = 0; s < numScvf; ++s)
		MatScale(Diffusion[s], m_imPorosityScvf[s], m_imMolDiffusionScvf[s]);

	// todo: Compute Darcy velocity for Dispersion
	// todo: Compute Dispersion
	// todo: Use DiffDisp = Diffusion + Dispersion

//	compute upwind shapes for transport equation
	if(!m_pUpwind->update(&geo, m_imDarcyVelScvf.values(), Diffusion, false))
	{
		UG_LOG("ERROR in 'DensityDrivenFlowElemDisc::assemble_JA': "
				"Cannot compute convection shapes.\n");
		return false;
	}

//	get a const (!!) reference to the upwind
	const IConvectionShapes<dim>& convShape
		= *const_cast<const IConvectionShapes<dim>*>(m_pUpwind);

// 	Loop Sub Control Volume Faces (SCVF)
	for(size_t ip = 0; ip < geo.num_scvf(); ++ip)
	{
	// 	Get current SCVF
		const typename FV1Geometry<TElem, dim>::SCVF& scvf = geo.scvf(ip);

		////////////////////////
		// Transport Equation
		////////////////////////

	//	Compute Convective Flux
		number flux = 0;
		for(size_t sh = 0; sh < scvf.num_sh(); ++sh)
			flux += convShape(ip, sh) * u(_C_, sh);

	//	Compute Diffusive Flux
		MatVecMult(Dgrad_c_ip, Diffusion[ip], m_imBrineGradScvf[ip]);
		const number diffFlux = VecDot(Dgrad_c_ip, scvf.normal());

	//	Sum total flux
		flux -= diffFlux;
		if(!m_BoussinesqTransport) flux *= m_imDensityScvf[ip];

	//	Add contribution to transport equation
		d(_C_,scvf.from()) += flux;
		d(_C_,scvf.to()) -= flux;

		////////////////////
		// Flow Equation
		////////////////////

	//	Compute flux
		flux = VecDot(m_imDarcyVelScvf[ip], scvf.normal());
		if(!m_BoussinesqFlow) flux *= m_imDensityScvf[ip];

	//	Add contribution to flow equation
		d(_P_,scvf.from()) += flux;
		d(_P_,scvf.to()) -= flux;
	}

//	we're done
	return true;
}


template<typename TDomain>
template<typename TElem >
bool
DensityDrivenFlowElemDisc<TDomain>::
assemble_JM(LocalMatrix& J, const LocalVector& u)
{
// 	get finite volume geometry
	static const FV1Geometry<TElem, dim>& geo =	Provider<FV1Geometry<TElem,dim> >::get();

// 	loop Sub Control Volumes (SCV)
	for(size_t ip = 0; ip < geo.num_scv(); ++ip)
	{
	// 	get current SCV
		const typename FV1Geometry<TElem, dim>::SCV& scv = geo.scv(ip);

	// 	get associated node
		const int co = scv.node_id();

	// 	Add to local matrix
		if(m_BoussinesqTransport)
			J(_C_, co, _C_, co) += m_imPorosityScv[ip] * scv.volume();
		else
			J(_C_, co, _C_, co) +=
				m_imPorosityScv[ip] * scv.volume() *
				(m_imDensityScv[ip]+m_imDensityScv.deriv(ip, _C_, co)*u(_C_, co));

		if(!m_BoussinesqFlow)
			J(_P_, co, _C_, co) += m_imPorosityScv[ip]
			                       * m_imDensityScv.deriv(ip, _C_, co) * scv.volume();
		//else
			//J(_P_, co, _C_, co) += 0;

		// Remark: Other addings are zero
		//J(_C_, co, _P_, co) += 0;
		//J(_P_, co, _P_, co) += 0;
	}

// 	we're done
	return true;
}


template<typename TDomain>
template<typename TElem >
bool
DensityDrivenFlowElemDisc<TDomain>::
assemble_M(LocalVector& d, const LocalVector& u)
{
// 	Get finite volume geometry
	static const FV1Geometry<TElem, dim>& geo =	Provider<FV1Geometry<TElem,dim> >::get();

// 	Loop Sub Control Volumes (SCV)
	for(size_t ip = 0; ip < geo.num_scv(); ++ip)
	{
	// 	Get current SCV
		const typename FV1Geometry<TElem, dim>::SCV& scv = geo.scv(ip);

	// 	Get associated node
		const int co = scv.node_id();

	// 	Add to local matrix
		if(m_BoussinesqTransport)
			d(_C_,co) += m_imPorosityScv[ip] * u(_C_,co) * scv.volume();
		else
			d(_C_,co) += m_imPorosityScv[ip] * m_imDensityScv[ip]
			                                 * u(_C_,co) * scv.volume();

		if(m_BoussinesqFlow)
			d(_P_,co) += m_imPorosityScv[ip] * scv.volume();
		else
			d(_P_,co) += m_imPorosityScv[ip] * m_imDensityScv[ip] * scv.volume();
	}

// 	we're done
	return true;
}


template<typename TDomain>
template<typename TElem >
bool
DensityDrivenFlowElemDisc<TDomain>::
assemble_f(LocalVector& d)
{
//	currently there is no contribution that does not depend on the solution

	return true;
}


////////////////////////////////////////////////////////////////////////////////
//	Constructor
////////////////////////////////////////////////////////////////////////////////

template<typename TDomain>
DensityDrivenFlowElemDisc<TDomain>::
DensityDrivenFlowElemDisc() :
	m_pUpwind(NULL), m_bConsGravity(true),
	m_BoussinesqTransport(true), m_BoussinesqFlow(true),
	m_imBrineScvf(false), m_imBrineGradScvf(false),
	m_imPressureGradScvf(false),
	m_imDensityScv(false), m_imDensityScvf(false),
	m_imDarcyVelScvf(false)
{
//	register assemble functions
	register_all_fv1_funcs();

//	register export
	m_exDarcyVel.add_needed_data(m_exBrine);
	register_export(m_exDarcyVel);
	register_export(m_exBrine);
	register_export(m_exBrineGrad);
	register_export(m_exPressureGrad);

//	register import
	register_import(m_imBrineScvf);
	register_import(m_imBrineGradScvf);
	register_import(m_imPressureGradScvf);
	register_import(m_imPorosityScvf);
	register_import(m_imPorosityScv);
	register_import(m_imPermeabilityScvf);
	register_import(m_imMolDiffusionScvf);
	register_import(m_imViscosityScvf);
	register_import(m_imDensityScv);
	register_import(m_imDensityScvf);
	register_import(m_imDarcyVelScvf);

//	connect to own export
	m_imBrineScvf.set_data(m_exBrine);
	m_imBrineGradScvf.set_data(m_exBrineGrad);
	m_imPressureGradScvf.set_data(m_exPressureGrad);
	m_imDarcyVelScvf.set_data(m_exDarcyVel);
}

////////////////////////////////////////////////////////////////////////////////
//	register assemble functions
////////////////////////////////////////////////////////////////////////////////

// register for 1D
template<typename TDomain>
void
DensityDrivenFlowElemDisc<TDomain>::
register_all_fv1_funcs()
{
//	get all grid element types in this dimension and below
	typedef typename domain_traits<dim>::DimElemList ElemList;

//	switch assemble functions
	boost::mpl::for_each<ElemList>( RegisterFV1(this) );
}

template<typename TDomain>
template<typename TElem>
void
DensityDrivenFlowElemDisc<TDomain>::
register_fv1_func()
{
	ReferenceObjectID id = geometry_traits<TElem>::REFERENCE_OBJECT_ID;
	typedef this_type T;
	static const int refDim = reference_element_traits<TElem>::dim;

	set_prep_elem_loop_fct(id, &T::template prepare_element_loop<TElem>);
	set_prep_elem_fct(	 id, &T::template prepare_element<TElem>);
	set_fsh_elem_loop_fct( id, &T::template finish_element_loop<TElem>);
	set_ass_JA_elem_fct(		 id, &T::template assemble_JA<TElem>);
	set_ass_JM_elem_fct(		 id, &T::template assemble_JM<TElem>);
	set_ass_dA_elem_fct(		 id, &T::template assemble_A<TElem>);
	set_ass_dM_elem_fct(		 id, &T::template assemble_M<TElem>);
	set_ass_rhs_elem_fct(	 id, &T::template assemble_f<TElem>);

	if(m_bConsGravity)
		m_exDarcyVel.template set_fct<T,refDim>(id, this, &T::template ex_darcy_cons_grav<TElem>);
	else
		m_exDarcyVel.template set_fct<T,refDim>(id, this, &T::template ex_darcy_std<TElem>);

	m_exBrine.		 template set_fct<T,refDim>(id, this, &T::template ex_brine<TElem>);
	m_exBrineGrad.	 template set_fct<T,refDim>(id, this, &T::template ex_brine_grad<TElem>);
	m_exPressureGrad.template set_fct<T,refDim>(id, this, &T::template ex_pressure_grad<TElem>);
}

////////////////////////////////////////////////////////////////////////////////
//	explicit template instantiations
////////////////////////////////////////////////////////////////////////////////

template class DensityDrivenFlowElemDisc<Domain1d>;
template class DensityDrivenFlowElemDisc<Domain2d>;
template class DensityDrivenFlowElemDisc<Domain3d>;

} // namespace ug

