
#ifndef __H__UG_BRIDGE__REGISTRY__
#define __H__UG_BRIDGE__REGISTRY__

#include <vector>
#include <string>
#include <map>
#include <cstring>
#include <typeinfo>
#include <boost/function.hpp>
#include <boost/type_traits.hpp>


#include "global_function.h"
#include "class.h"
#include "param_to_type_value_list.h"
#include "parameter_stack.h"

namespace ug
{
namespace bridge
{

//	PREDECLARATIONS
class Registry;

///	declaration of registry callback function.
/**	Allows to notify listeners if the registry changes.
 * Since FuncRegistryChanged is a functor, you can either
 * pass a normal function or a member function of a class
 * (Have a look at boost::bind in the second case).
 */
typedef boost::function<void (Registry* pReg)> FuncRegistryChanged;

// Registry
/** registers functions and classes that are exported to scripts and visualizations
 * It also allows to register callbacks that are called if the
 * registry changes.
 *
 * Please note that once a class or method is registered at the
 * registry, it will can not be removed (This is important for the
 * implementation of callbacks).
 */
class Registry {
	public:
		Registry()	{}
		
	////////////////////////
	//	callbackst
	////////////////////////
	///	adds a callback which is triggered whenever Registry::registry_changed is called.
		void add_callback(FuncRegistryChanged callback)
		{
			m_callbacksRegChanged.push_back(callback);
		}
		
		void registry_changed()
		{
			check_consistency();
		//	iterate through all callbacks and call them
			for(size_t i = 0; i < m_callbacksRegChanged.size(); ++i){
				m_callbacksRegChanged[i](this);
			}
		}

	//////////////////////
	// global functions
	//////////////////////
		
	/**	References the template function proxy_function<TFunc> and stores
	 * it with the FuntionWrapper.
	 */
		template<class TFunc>
		Registry& add_function(const char* funcName, TFunc func, const char* group = "",
								const char* retValInfos = "", const char* paramInfos = "",
								const char* tooltip = "", const char* help = "")
		{
		//	At this point the method name contains parameters (name|param1=...).
		//todo: they should be removed and specified with an extra parameter.

			std::string strippedMethodName = funcName;
			std::string methodOptions;
			std::string::size_type pos = strippedMethodName.find("|");
			if(pos != std::string::npos){
				methodOptions = strippedMethodName.substr(pos + 1, strippedMethodName.length() - pos);
				strippedMethodName = strippedMethodName.substr(0, pos);
				UG_LOG(strippedMethodName << " ... | ... " << methodOptions << std::endl);
			}

		//	trim whitespaces
			{
				const size_t start = strippedMethodName.find_first_not_of(" \t");
				const size_t end = strippedMethodName.find_last_not_of(" \t");
				if(start != std::string::npos && end != std::string::npos)
					strippedMethodName = strippedMethodName.substr(start, end - start + 1);
			}
			{
				const size_t start = methodOptions.find_first_not_of(" \t");
				const size_t end = methodOptions.find_last_not_of(" \t");
				if(start != std::string::npos && end != std::string::npos)
					methodOptions = methodOptions.substr(start, end - start + 1);
			}

		// 	check that name is not empty
			if(strippedMethodName.empty())
			{
				std::cout << "### Registry ERROR: Trying to register empty function name."
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(strippedMethodName.c_str()));
			}

		//	if the function is already in use, we have to add an overload
			ExportedFunctionGroup* funcGrp = get_exported_function_group(strippedMethodName.c_str());
			if(!funcGrp)
			{
			//	we have to create a new function group
				funcGrp = new ExportedFunctionGroup(strippedMethodName.c_str());
				m_vFunction.push_back(funcGrp);
			}

		//  add an overload to the function group
			bool success = funcGrp->add_overload(func, &FunctionProxy<TFunc>::apply,
												methodOptions.c_str(), group,
												retValInfos, paramInfos,
												tooltip, help);

			if(!success){
				std::cout << "### Registry ERROR: Trying to register function name '" << funcName
						<< "', that is already used by another function in this registry."
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(strippedMethodName.c_str()));
			}
	
			return *this;
		}

	/// number of functions registered at the Registry (overloads are not counted)
		size_t num_functions() const							{return m_vFunction.size();}

	/// returns the first overload of an exported function
		ExportedFunction& get_function(size_t ind)	 			{return *m_vFunction.at(ind)->get_overload(0);}

	///	returns the number of overloads of a function
		size_t num_overloads(size_t ind)						{return m_vFunction.at(ind)->num_overloads();}

	///	returns the i-th overload of a function
		ExportedFunction& get_overload(size_t funcInd, size_t oInd)	{return *m_vFunction.at(funcInd)->get_overload(oInd);}

	///	returns a group which contains all overloads of a function
		ExportedFunctionGroup& get_function_group(size_t ind) 		{return *m_vFunction.at(ind);}

	///////////////////
	// classes
	///////////////////

	/** Register a class at this registry
	 * This function registers any class
	 */
		template <typename TClass>
		ExportedClass_<TClass>& add_class_(const char* className, const char* group = "", const char *tooltip="")
		{
		//	check that className is not already used
			if(classname_registered(className))
			{
				std::cout << "### Registry ERROR: Trying to register class name '" << className
						<< "', that is already used by another class in this registry."
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(className));
			}
		// 	check that name is not empty
			if(strlen(className) == 0)
			{
				std::cout << "### Registry ERROR: Trying to register empty class name."
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(className));
			}

		//	new class pointer
			ExportedClass_<TClass>* newClass = NULL;

		//	try creation
			try
			{
				newClass = new ExportedClass_<TClass>(className, group, tooltip);
			}
			catch(ug::bridge::UG_REGISTRY_ERROR_ClassAlreadyNamed ex)
			{
				std::cout << "### Registry ERROR: Trying to register class with name '" << className
						<< "', that has already been named. This is not allowed. "
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(className));
			}

		//	add new class to list of classes
			m_vClass.push_back(newClass);

			return *newClass;
		}

	///	performs some checks, throws error if something wrong
		template <typename TClass, typename TBaseClass>
		void check_base_class(const char* className)
		{
		//	check that className is not already used
			if(classname_registered(className))
			{
				std::cout << "### Registry ERROR: Trying to register class name '" << className
						<< "', that is already used by another class in this registry."
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(className));
			}
		// 	check that name is not empty
			if(strlen(className) == 0)
			{
				std::cout << "### Registry ERROR: Trying to register empty class name."
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(className));
			}

		// 	check that base class is not same type as class
			if(typeid(TClass) == typeid(TBaseClass))
			{
				std::cout << "### Registry ERROR: Trying to register class " << className
						<< "\n### that derives from itself. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(className));
			}

		// 	check that class derives from base class
			if(boost::is_base_of<TBaseClass, TClass>::value == false)
			{
				std::cout << "### Registry ERROR: Trying to register class " << className
						<< "\n### with base class that is no base class. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(className));
			}
		}

	/** Register a class at this registry
	 * This function registers any class together with its base class
	 */
		template <typename TClass, typename TBaseClass>
		ExportedClass_<TClass>& add_class_(const char* className, const char* group = "", const char *tooltip = "")
		{
		//	check
			check_base_class<TClass, TBaseClass>(className);

		//	new class pointer
			ExportedClass_<TClass>* newClass = NULL;

		//	try creation of new class
			try { newClass = new ExportedClass_<TClass>(className, group, tooltip);}
			catch(ug::bridge::UG_REGISTRY_ERROR_ClassAlreadyNamed ex)
			{
				std::cout << "### Registry ERROR: Trying to register class with name '" << className
						<< "', that has already been named. This is not allowed. "
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(className));
			}

		// 	set base class names
			try
			{
				ClassNameProvider<TClass>::template set_name<TBaseClass>(className, group);
			}
			catch(ug::bridge::UG_REGISTRY_ERROR_ClassUnknownToRegistry ex)
			{
				std::cout <<"### Registry ERROR: Trying to register class with name '" << className
						<< "', that derives from class, that has not yet been registered to this Registry."
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(className));
			}

		//	add cast function
			ClassCastProvider::add_cast_func<TBaseClass, TClass>();

		//	add new class to list of classes
			m_vClass.push_back(newClass);
			return *newClass;
		}

	/** Register a class at this registry
	 * This function registers any class together with its base class
	 */
		template <typename TClass, typename TBaseClass1, typename TBaseClass2>
		ExportedClass_<TClass>& add_class_(const char* className, const char* group = "")
		{
		//	check
			check_base_class<TClass, TBaseClass1>(className);
			check_base_class<TClass, TBaseClass2>(className);

		//	new class pointer
			ExportedClass_<TClass>* newClass = NULL;

		//	try creation of new class
			try { newClass = new ExportedClass_<TClass>(className, group);}
			catch(ug::bridge::UG_REGISTRY_ERROR_ClassAlreadyNamed ex)
			{
				std::cout << "### Registry ERROR: Trying to register class with name '" << className
						<< "', that has already been named. This is not allowed. "
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(className));
			}

		// 	set base class names
			try
			{
				ClassNameProvider<TClass>::template set_name<TBaseClass1, TBaseClass2>(className, group);
			}
			catch(ug::bridge::UG_REGISTRY_ERROR_ClassUnknownToRegistry ex)
			{
				std::cout <<"### Registry ERROR: Trying to register class with name '" << className
						<< "', that derives from class, that has not yet been registered to this Registry."
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(className));
			}

		//	add cast function
			ClassCastProvider::add_cast_func<TBaseClass1, TClass>();
			ClassCastProvider::add_cast_func<TBaseClass2, TClass>();

		//	add new class to list of classes
			m_vClass.push_back(newClass);
			return *newClass;
		}

	/**
	 * Get Reference to already registered class
	 */
		template <typename TClass>
		ExportedClass_<TClass>& get_class_()
		{
			const char* name;
		// get class names
			try
			{
				name = ClassNameProvider<TClass>::name();
			}
			catch(ug::bridge::UG_REGISTRY_ERROR_ClassUnknownToRegistry ex)
			{
				std::cout <<"### Registry ERROR: Trying to get class "
						<< "that has not yet been named."
						<< "\n### Please change register process. Aborting ..." << std::endl;
				throw(UG_REGISTRY_ERROR_RegistrationFailed(""));
			}

		//	look for class in this registry
			for(size_t i = 0; i < m_vClass.size(); ++i)
			{
			//  compare strings
				if(strcmp(name, m_vClass[i]->name()) == 0)
					return *dynamic_cast<ExportedClass_<TClass>* >(m_vClass[i]);
			}

		//	not found
			std::cout <<"### Registry ERROR: Trying to get class with name '" << name
					<< "', that has not yet been registered to this Registry."
					<< "\n### Please change register process. Aborting ..." << std::endl;
			throw(UG_REGISTRY_ERROR_RegistrationFailed(name));
		}

	/// number of classes registered at the Registry
		size_t num_classes() const						{return m_vClass.size();}

	/// returns an exported function
		const IExportedClass& get_class(size_t ind)	const {return *m_vClass.at(ind);}

		bool check_consistency()
		{
			size_t found = 0;
			for(size_t i=0; i<num_functions(); i++){
				ExportedFunctionGroup& funcGrp = get_function_group(i);
				for(size_t j = 0; j < funcGrp.num_overloads(); ++j){
					if(funcGrp.get_overload(j)->check_consistency()) found++;
				}
			}

			// check classes
			for(size_t i=0; i<num_classes(); i++)
			{
				const bridge::IExportedClass &c = get_class(i);
				for(size_t j=0; j<c.num_methods(); j++)
					if(c.get_method(j).check_consistency(c.name())) found++;
				for(size_t j=0; j<c.num_const_methods(); j++)
					if(c.get_const_method(j).check_consistency(c.name())) found++;
			}

			UG_ASSERT(found == 0, "ERROR: " << found << " functions are using undeclared classes\n");
			if(found > 0) return false;
			else return true;
		}

	/// destructor
		~Registry()
		{
		//  delete registered functions
			for(size_t i = 0; i < m_vFunction.size(); ++i)
			{
				if(m_vFunction[i] != NULL)
					delete m_vFunction[i];
			}
		//  delete registered classes
			for(size_t i = 0; i < m_vClass.size(); ++i)
			{
				if(m_vClass[i] != NULL)
					delete m_vClass[i];
			}
		}

	protected:
		// returns true if classname is already used by a class in this registry
		bool classname_registered(const char* name)
		{
			for(size_t i = 0; i < m_vClass.size(); ++i)
			{
			//  compare strings
				if(strcmp(name, m_vClass[i]->name()) == 0)
					return true;
			}
			return false;
		}

		// returns true if functionname is already used by a function in this registry
		bool functionname_registered(const char* name)
		{
			for(size_t i = 0; i < m_vFunction.size(); ++i)
			{
			//  compare strings
				if(strcmp(name, (m_vFunction[i]->name()).c_str()) == 0)
					return true;
			}
			return false;
		}

		ExportedFunctionGroup* get_exported_function_group(const char* name)
		{
			for(size_t i = 0; i < m_vFunction.size(); ++i)
			{
			//  compare strings
				if(strcmp(name, (m_vFunction[i]->name()).c_str()) == 0)
					return m_vFunction[i];
			}
			return NULL;
		}

	private:
	//	disallow copy
		Registry(const Registry& reg)	{}
		
	//	registered functions
		std::vector<ExportedFunctionGroup*>	m_vFunction;

	//	registered classes
		std::vector<IExportedClass*> m_vClass;

	//	Callback, that are called when registry changed is invoked
		std::vector<FuncRegistryChanged> m_callbacksRegChanged;
};

} // end namespace registry

} // end namespace ug


#endif /* __H__UG_BRIDGE__REGISTRY__ */
