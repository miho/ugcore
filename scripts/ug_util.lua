--[[!
-- \defgroup scripts_util Lua Utility Scripts
-- \ingroup scripts
-- General Lua utility scripts for ug4.
-- \{
]]--

-- Create util namespace
util = util or {}

ug_load_script("util/meta_util.lua")
ug_load_script("util/test_utils.lua")
ug_load_script("util/domain_distribution_util.lua")
ug_load_script("util/stats_util.lua")
ug_load_script("util/user_data_util.lua")
ug_load_script("util/gnuplot.lua")
ug_load_script("util/table_util.lua")
ug_load_script("util/time_step_util.lua")
ug_load_script("util/solver_util.lua")
ug_load_script("util/domain_disc_util.lua")

--------------------------------------------------------------------------------

--! use it like ug_assert(numPreRefs <= numRefs, "It must be choosen: numPreRefs <= numRefs")
--! @param condition the condition to assert
--! @param msg message to be printed if condition is not fulfilled
function ug_assert(condition, msg)
	if condition then
		return
	else
		print("BACKTRACE:")
		DebugBacktrace()
		print("ASSERTION FAILED:")
		local f, l = test.getSourceAndLine()
		print("     File:      "..f)
		print("     Line:      "..l)
		print("     Message:   "..msg)
		assert(false)
	end
end

--------------------------------------------------------------------------------

--! returns the standard path at which grids are stored
function util.GetGridPath()
	return ug_get_data_path().."/grids/"
end

function util.GlobalRefineParallelDomain(domain)
	local dim = domain:get_dim()
	if dim == 1 then
		return GlobalRefineParallelDomain1d(domain)
	elseif dim == 2 then
		return GlobalRefineParallelDomain2d(domain)
	elseif dim == 3 then
		return GlobalRefineParallelDomain3d(domain)
	end
	return false
end

--------------------------------------------------------------------------------
-- User Data utils
--------------------------------------------------------------------------------

--! creates a Const User Matrix 2d 
function util.CreateConstUserMatrix2d(m00, m01, m10, m11)
	local mat = ConstUserMatrix2d()
	mat:set_entry(0, 0, m00)
	mat:set_entry(0, 1, m01)
	mat:set_entry(1, 0, m10)
	mat:set_entry(1, 1, m11)	
	return mat
end

--! creates a Const User Matrix 3d
function util.CreateConstUserMatrix3d(m00, m01, m02, m10, m11, m12, m20, m21, m22)
	local mat = ConstUserMatrix3d()
	mat:set_entry(0, 0, m00)
	mat:set_entry(0, 1, m01)
	mat:set_entry(0, 2, m02)
	mat:set_entry(1, 0, m10)
	mat:set_entry(1, 1, m11)
	mat:set_entry(1, 2, m12)
	mat:set_entry(2, 0, m20)
	mat:set_entry(2, 1, m21)
	mat:set_entry(2, 2, m22)	
	return mat
end

--! creates a Const User Vector 2d
function util.CreateConstUserVector2d(v0, v1)
	local vec = ConstUserVector2d()
	vec:set_entry(0, v0)
	vec:set_entry(1, v1)
	return vec
end

--! creates a Const User Vector 3d
function util.CreateConstUserVector3d(v0, v1, v2)
	local vec = ConstUserVector3d()
	vec:set_entry(0, v0)
	vec:set_entry(1, v1)
	vec:set_entry(2, v2)
	return vec
end

--------------------------------------------------------------------------------
-- Subset utils
--------------------------------------------------------------------------------

--! util.CheckSubsets
--! checks if all required subsets are contained in the SubsetHandler
--! @param dom Domain
--! @param neededSubsets List of subsets the SubsetHandler must contain
--! @return true if all subsets are contained, false else
function util.CheckSubsets(dom, neededSubsets)
	sh = dom:subset_handler()
	for i, tval in ipairs(neededSubsets) do
		if sh:get_subset_index(tval) == -1 then
			print("Domain does not contain subset '"..tval.."'.")
			return false
		end
	end
	
	return true
end


--! Creates a new domain and loads the specified grid. The method then performs
--! numRefs global refinements.
--! A list of subset-names can be specified which have to be present in the loaded grid.
--! The method returns the created domain.
--! @note Some paramters are optional. nil is a valid value for each optional parameter.
--! @return	(Domain) the created domain
--! @param gridName	(string) The filename of the grid which shall be loaded.
--!					The grid is searched in a path relative to the current path
--!					first. If it isn't found there, the path is interpreted as
--!					an absolute path. If the grid still can't be found, the method
--!					tries to load it from UG_BASE/data/grids.
--! @param numRefs	(int) The total number of global refinements
--! @param numPreRefs	(int) The number of refinements that are performed before
--!						distribution.
--! @param neededSubsets	(optional, list of strings) The subsets that are required
--!							by the simulation. If not all those subsets are present,
--!							the method aborts. Default is an empty list.
function util.CreateDomain(gridName, numRefs, neededSubsets)

	-- create Instance of a Domain
	local dom = Domain()
	
	-- load domain
	write("Loading Domain "..gridName.." ... ") 
	LoadDomain(dom, gridName)
	write("done. ")
	
	-- Create a refiner instance. This is a factory method
	-- which automatically creates a parallel refiner if required.
	if numRefs > 0 then
		write("Refining("..numRefs.."): ")
		local refiner = GlobalDomainRefiner(dom)
		for i=1,numRefs do
			refiner:refine()
			write(i .. " ")
		end
		write("done.")
		delete(refiner)
	end
	write("\n")
	
	-- check whether required subsets are present
	if neededSubsets ~= nil then
		if util.CheckSubsets(dom, neededSubsets) == false then 
			print("Something wrong with required subsets. Aborting.");
			exit();
		end
	end
	
	-- return the created domain
	return dom
end


--! Creates a new domain and loads the specified grid. The method then performs
--! numPreRefs refinements before it distributes the domain onto the available
--! processes. The partitioning method can be chosen through distributionMethod.
--! After distribution the domain is refined until a total of numRefs refinement
--! steps has been performed (including numPreRefs).
--! A list of subset-names can be specified. After distribution the methods checks
--! Whether all processes received the required subsets.
--! The method returns the created domain.
--! @note Some paramters are optional. nil is a valid value for each optional parameter.
--! @return	(Domain) the created domain
--! @param gridName	(string) The filename of the grid which shall be loaded.
--!					The grid is searched in a path relative to the current path
--!					first. If it isn't found there, the path is interpreted as
--!					an absolute path. If the grid still can't be found, the method
--!					tries to load it from UG_BASE/data/grids.
--! @param numRefs	(int) The total number of global refinements
--! @param numPreRefs	(int) The number of refinements that are performed before
--!						distribution.
--! @param neededSubsets	(optional, list of strings) The subsets that are required
--!							by the simulation. If not all those subsets are present,
--!							the method aborts. Default is an empty list.
--! @param distributionMethod	(optional, string) The distribution method.
--!								Either "bisection" or "metis". Default is "bisection".
--!								See util.DistributeDomain for more information
--!								(in UG_BASE/scripts/util/domain_distribution.lua)
--! @param verticalInterfaces	(optional, bool) Vertical interfaces are required
--!								by multi-grid solvers. Default is true.
--!								See util.DistributeDomain for more information
--!								(in UG_BASE/scripts/util/domain_distribution.lua)
--! @param numTargetProcs	(optional, int) The number of target processes to which
--!							the domain shall be distributed. Make sure that the
--!							number of target processes is not higher than the
--!							number of elements in the distributionLevel.
--!							Default is GetNumProcesses()
--!							See util.DistributeDomain for more information
--!							(in UG_BASE/scripts/util/domain_distribution.lua)
--! @param distributionLevel	(optional, int) The level on which the distribution
--!								is performed. Default is the domains top-level
--!								after pre-refinement.
--!								See util.DistributeDomain for more information
--!								(in UG_BASE/scripts/util/domain_distribution.lua)
--! @param wFct 			(optional SmartPtr\<EdgeWeighting\>) Sets the weighting function for the
--!							'metisReweigh' partitioning method.
function util.CreateAndDistributeDomain(gridName, numRefs, numPreRefs,
										neededSubsets, distributionMethod,
										verticalInterfaces, numTargetProcs,
										distributionLevel, wFct)

	-- create Instance of a Domain
	local dom = Domain()
	
	-- load domain
	write("Loading Domain "..gridName.." ... ") 
	LoadDomain(dom, gridName)
	write("done. ")
	
	-- create Refiner
	if numPreRefs > numRefs then
		print("numPreRefs must be smaller than numRefs. Aborting.");
		exit();
	end
	
	if numPreRefs > numRefs then
		numPreRefs = numRefs
	end
	
	-- Create a refiner instance. This is a factory method
	-- which automatically creates a parallel refiner if required.
	local refiner = nil
	if numRefs > 0 then
		refiner = GlobalDomainRefiner(dom)
	end
	
	write("Pre-Refining("..numPreRefs.."): ")
	-- Performing pre-refines
	for i=1,numPreRefs do
		write(i .. " ")
		refiner:refine()
	end
	write("done. Distributing...")
	-- Distribute the domain to all involved processes
	if util.DistributeDomain(dom, distributionMethod, verticalInterfaces, numTargetProcs, distributionLevel, wFct) == false then
		print("Error while Distributing Grid. Aborting.")
		exit();
	end
	write(" done. Post-Refining("..(numRefs-numPreRefs).."): ")
	
	-- Perform post-refine
	for i=numPreRefs+1,numRefs do
		refiner:refine()
		write(i-numPreRefs .. " ")
	end
	write("done.\n")
	
	-- Now we loop all subsets an search for it in the SubsetHandler of the domain
	if neededSubsets ~= nil then
		if util.CheckSubsets(dom, neededSubsets) == false then 
			print("Something wrong with required subsets. Aborting.");
			exit();
		end
	end
	
	
	--clean up
	if refiner ~= nil then
		delete(refiner)
	end
	
	-- return the created domain
	return dom
end


--------------------------------------------------------------------------------
-- some auxiliary functions
--------------------------------------------------------------------------------
--! function returns true if the number is a power of two
function util.IsPowerOfTwo(n)
	local number compare = 1

	while (compare < n) do
		compare = compare*2
	end

	return compare==n
end

--! function returns true if the number is a natural number
function util.IsNaturalNumber(n)
	if n-math.floor(n) == 0 then
		return true
	else
		return false
	end
end

--! function to factorise number which has to be a power of 2 in two factors
--! which differ at most by a factor of 2 and returns both
--! (first the smaller one, then the larger one).
function util.FactorizeInPowersOfTwo(n)
	if not util.IsPowerOfTwo(n) then
		print("Number to factorise must be a power of 2. Aborting.")
		exit()
	end

	local number firstFactor = n
	local number secFactor = 1

	while (firstFactor > 2*secFactor) do
		firstFactor = firstFactor/2
		secFactor = secFactor*2
	end

	return secFactor, firstFactor
end

--------------------------------------------------------------------------------
-- Command line functions
--------------------------------------------------------------------------------

function util.ConcatOptions(options)
	local sOpt = ""
	if options ~= nil then
		sOpt = " ["
		for i=1,#options do
			if i > 1 then sOpt = sOpt.." | " end
			sOpt = sOpt..options[i]
		end
		sOpt = sOpt.."]"
	end
	return sOpt
end

function util.CheckOptionsType(name, options, atype)
   if options ~= nil then
		for i=1,#options do
			ug_assert(type(options[i]) == atype, "ERROR in util.GetParam: passed option '"..options[i]..
		    			"' for '"..name.."' not a "..atype)
		end
	end
end

function util.CheckOptionsValue(name, value, options)
	if options ~= nil then
		local bValid = false
		for i=1,#options do
			if value == options[i] then bValid = true; end
		end
		ug_assert(bValid, "ERROR in util.GetParam: passed value '"..value.."' for '"
				..name.."' not contained in options:"..util.ConcatOptions(options))
	end
end

util.args = util.args or {}
util.argsUsed = util.argsUsed or {}

--! util.GetParam
--! returns parameter in ugargv after ugargv[i] == name
--! @param name parameter in ugargv to search for
--! @param default returned value if 'name' is not present (default nil)
--! @param description description for 'name' (default nil)
--! @param options a table of options e.g. {"jac", "sgs"}
--! @param atype type of the parameter, e.g. "number", "string", "boolean". default "string"
--! @return parameter in ugargv after ugargv[i] == name or default if 'name' was not present
function util.GetParam(name, default, description, options, atype)

	-- check options
    if options ~= nil then
	    ug_assert(type(options) == "table",
	    	"ERROR in util.GetParam: passed options for '"..name.."' not a table.")
	    
	    if atype == nil then
	    	util.CheckOptionsType(name, options, "string")
	    end
    end

	-- store infos
	util.args[name] = {}
	util.args[name].description = (description or "")
	util.args[name].default = default	
	util.args[name].type = (atype or " (string) ")
	util.args[name].options = options

	-- check if argument passed
	local value = default
	local bFound = false
	for i = 1, ugargc-1 do
		if not(bFound) and ugargv[i] == name then			
			util.argsUsed[i]=true
			util.argsUsed[i+1]=true
			value = ugargv[i+1]
			bFound=true
		end
	end
	util.args[name].value = value
	
	-- check options
	if atype == nil then
		util.CheckOptionsValue(name, value, options)
	end
	
	-- return default
	return value; 
end


--! util.GetParamNumber
--! use with CommandLine to get option, like -useAMG
--! if parameter is not a number, returns default
--! @param name parameter in ugargv to search for
--! @param default returned value if 'name' is not present (default nil)
--! @param description description for 'name' (default nil)
--! @return the number after the parameter 'name' or default if 'name' was not present/no number
function util.GetParamNumber(name, default, description, options)

	-- check options
	util.CheckOptionsType(name, options, "number")
	
	-- read in
	local param = util.GetParam(name, default, description, options, " (number) ")
	ug_assert(param ~= nil, "ERROR in GetParamNumber: Number Parameter "..name.." not set and no default value given.")
	-- cast to number	
	local value = tonumber(param)
	ug_assert(value ~= nil, "ERROR in GetParamNumber: passed '"..param.."' for Parameter '"
				..name.."' is not a number.")

	-- check options
	util.CheckOptionsValue(name, value, options)			
	
	-- return value
	return value
end

--! util.GetParamBool
--! use with CommandLine to get boolean option, like -useAMG true
--! @param default returned value if 'name' is not present (default nil)
--! @param description description for 'name' (default nil)
--! @return the number after the parameter 'name' or default if 'name' was not present/no number
--! unlike util.HasParamOption , you must specify a value for your optionn, like -useAMG 1
--! possible values are (false): 0, n, false, (true) 1, y, j, true
function util.GetParamBool(name, default, description)

	local r = util.GetParam(name, tostring(default), description, nil, "  (bool)  ")
	
	if r == "0" or r == "false" or r == "n" then
		return false
	elseif r == "1" or r == "true" or r == "y" or r == "j" then
		return true
	else
		print("ERROR in GetParamBool: passed '"..r.."' for Parameter '"..name.."' is not a bool.")
		exit();
	end
end

--! util.HasParamOption
--! use with CommandLine to get option, like -useAMG
--! @param name option in argv to search for
--! @param description description for 'name' (default nil)
--! @return true if option found, else false
function util.HasParamOption(name, description)

	-- store infos
	util.args[name] = {}
	util.args[name].description = (description or "")
	util.args[name].default = "false"	
	util.args[name].type = " [option] "
	util.args[name].value = "false"

	-- check if passed
	for i = 1, ugargc do
		if ugargv[i] == name then
			util.argsUsed[i]=true
			util.args[name].value = "true"
			return true
		end		
	end	
	
	-- not passed
	return false 
end

--! returns all arguments from the command line
function util.GetCommandLine()
	local pline = ""
	for i=1, ugargc do
		pline = pline..ugargv[i].." "
	end
	return pline
end

--! lists all the command line arguments which where used or could
--! have been used with util.GetParam, util.GetParamNumber and util.HasParamOption
function util.PrintArguments()
	local length=0
	for name,arg in pairsSortedByKeys(util.args) do
		length = math.max(string.len(name), length)
	end	
	for name,arg in pairsSortedByKeys(util.args) do
		print("  "..util.adjuststring(name, length, "l").." = "..arg.value)
	end
end

--! prints out the description to each GetParam-parameter so far called
function util.PrintHelp()
	local length=0
	for name,arg in pairsSortedByKeys(util.args) do
		length = math.max(string.len(name), length)
	end	
	for name,arg in pairsSortedByKeys(util.args) do
		sOpt = util.ConcatOptions(arg.options)
		print(arg.type..util.adjuststring(name, length, "l").." = "..arg.value..
			  " : "..arg.description..sOpt.." (default = "..arg.default..")")
	end
end

--! calls util.PrintHelp() and exits if HasParamOption("-help")
--! @param optional description of the script
function util.CheckAndPrintHelp(desc)
	if util.HasParamOption("-help", "print this help") then
		if desc ~= nil then print(desc) end
		print()			
		util.PrintHelp()
		exit()
	end
end

--! lists all command line arguments which were provided but could not be used.
function util.PrintIgnoredArguments()
	if bPrintIgnoredArgumentsCalled then return end
	bPrintIgnoredArgumentsCalled = true
	local pline = ""
	for i=1, ugargc do
		if (util.argsUsed == nil or util.argsUsed[i] == nil) and 
			string.sub(ugargv[i], 1,1) == "-" 
			and string.sub(ugargv[i], 1,8) ~= "-outproc"
			and string.sub(ugargv[i], 1,7) ~= "-noterm"
			and string.sub(ugargv[i], 1,10) ~= "-logtofile"
			and string.sub(ugargv[i], 1,7) ~= "-noquit" then
			local imin=10
			local namemin=""
			for name,arg in pairs(util.args) do
				if name == ugargv[i] then
					imin = 0
				else
					local d = LevenshteinDistance(name, ugargv[i])
					if d < imin then
						imin = d
						namemin = name
					end
				end
			end
			if imin == 0 then
				pline=pline..ugargv[i].." [specified multiple times. Used value: "..util.args[ugargv[i]].value.."]\n"
			elseif imin < string.len(ugargv[i])/2 then
				pline=pline..ugargv[i].." [did you mean "..namemin..util.args[namemin].type.."?]\n"
			else
				pline=pline..ugargv[i].." "				
			end
		end
	end
	if pline ~= "" then
		print("WARNING: Ignored arguments (or not parsed with util.GetParam/util.GetParamNumber/util.HasParamOption) :\n"..pline.."\n")
	end
end

--------------------------------------------------------------------------------
-- lua script functions
--------------------------------------------------------------------------------

function util.PrintTableHelper(indexPar, valuePar)
	if type(valuePar) == "table" then
		print(util.PrintTableHelperIntend .. tostring(indexPar)  .. " = {")
		util.PrintTableHelperIntend = util.PrintTableHelperIntend .. "  "
		
		for i,v in pairs(valuePar) do util.PrintTableHelper(i, v) end
		
		util.PrintTableHelperIntend = string.sub(util.PrintTableHelperIntend, 3)
		print(util.PrintTableHelperIntend .. "}")
	else
		if type(valuePar) == "string" or type(valuePar) == "number" then
			print(util.PrintTableHelperIntend .. tostring(indexPar) .. " = " .. valuePar )
		elseif type(valuePar) == "boolean" then
			print(util.PrintTableHelperIntend .. tostring(indexPar) .. " = " .. tostring(valuePar) )
		else
			print(util.PrintTableHelperIntend .. "type(" .. tostring(indexPar) .. ") = " .. type(valuePar) )
		end
	end
end

--! to print tables
function util.PrintTable(tablePar)
	util.PrintTableHelperIntend = ""
	util.PrintTableHelper("", tablePar)
end



--! pairsSortedByKeys
--! the normal pairs(table) function returns elements unsorted
--! this function goes through elements sorted.
--! see http://www.lua.org/pil/19.3.html
function pairsSortedByKeys (t, f)
    local a = {}
    for n in pairs(t) do 
        table.insert(a, n) 
    end
    table.sort(a, f)
    local i = 0      -- iterator variable
    local function iter()   -- iterator function
    	i = i + 1
        if a[i] == nil then return nil
        else return a[i], t[a[i]]
        end
    end
	return iter
end

--------------------------------------------------------------------------------
-- basic functions missing lua
--------------------------------------------------------------------------------

--! adds writeln 
function writeln(...)
	write(...)
	write("\n")
end


function formatf(s, ...)
	return s:format(...)
end

function printf(s,...)
	print(formatf(...))
end

--! fsize
--! returns the filesize of a file (http://www.lua.org/pil/21.3.html)
--! @param file
--! @return filesize
function fsize (file)
	local current = file:seek()      -- get current position
    local size = file:seek("end")    -- get file size
    file:seek("set", current)        -- restore position
    return size
end

function bool2string(boolB)
	if boolB then
		return "true"
	else
		return "false"
	end
end

--------------------------------------------------------------------------------
-- list and free user data
--------------------------------------------------------------------------------

function ListUserDataInTable(t, name)
   	for n,v in pairs(t) do
      if type(v) == "userdata" then
		 print(name.."["..n.."]")
      end
  
	  if type(v) == "table" then
		if(n ~= "_G" and n ~= "io" and n ~= "package" and n ~= "gnuplot") then 
			ListUserDataInTable(v, name.."["..n.."]")
		end
  	  end
    end
end

--! Lists all user data (even in tables)
function ListUserData()
   	for n,v in pairs(_G) do
	   -- all userdata
	   if type(v) == "userdata" then
		 print(n)
	   end
    
	    -- userdata in table
		if type(v) == "table" then
			if(n ~= "_G" and n ~= "io" and n ~= "package" and n ~= "gnuplot") then 
				ListUserDataInTable(_G[n], n)
			end
		end
    end
end

function FreeUserDataInTable(t)
   	for n,v in pairs(t) do
      if type(v) == "userdata" then
      	 t[n] = nil
      end
      
      if type(v) == "table" then
		if(n ~= "_G" and n ~= "io" and n ~= "package" and n ~= "gnuplot") then
			FreeUserDataInTable(v)
		end
  	  end
      
    end
end

--! sets all userdata to nil (even in tables) and calls garbage collector
function FreeUserData()
   -- set user data to nil
   for n,v in pairs(_G) do
      if type(v) == "userdata" then
		 _G[n] = nil
      end
      
      if type(v) == "table" then
		if(n ~= "_G" and n ~= "io" and n ~= "package" and n ~= "gnuplot") then
 	     	FreeUserDataInTable(_G[n])
 	     end
      end
   end
   
   -- call garbage collector
   collectgarbage("collect")
end

--! 
--! @param pluginNamesList a list like {"amg", "d3f"} of plugins to check
function RequiredPlugins(pluginNamesList)
	local notLoadedNames = ""
	local cmakePluginString = ""
	for i,v in pairs(pluginNamesList) do
		if PluginLoaded(v) == false then
			notLoadedNames=notLoadedNames..v.." "
			cmakePluginString = cmakePluginString.." -D"..v.."=ON"
		end
	end	
	if notLoadedNames:len() > 0 then
		print("Plugin(s) needed but not loaded: "..notLoadedNames)		
		print("Please use \n   cmake "..cmakePluginString.." ..; make\nin your build directory to add the plugin(s).")
		exit()
	end
	
end

function AssertPluginsLoaded(pluginNamesList)
	RequiredPlugins(pluginNamesList)
end

function util.GetUniqueFilenameFromCommandLine()
	local ret=""
	for i = 1, ugargc do
		ret = ret.." "..ugargv[i]		
	end
	ret = FilenameStringEscape(ret)
	if GetNumProcesses() > 1 then
		return ret.."_numProcs_"..GetNumProcesses()
	else
		return ret
	end
end



-- end group scripts_util
--[[!  
\} 
]]--
