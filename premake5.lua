workspace "JsonParser"
  configurations { "Debug", "Release" }

project "JsonParserLib"
  kind "StaticLib"
  language "C++"
  cppdialect "C++17"
  
  staticruntime "off"

  targetdir ("Build/Bin/%{prj.name}/%{cfg.longname}")
  objdir ("Build/Obj/%{prj.name}/%{cfg.longname}")

  includedirs "include/"
  files {        "include/**.h",
                  "src/**.cpp" }
   
  filter "configurations:Debug"
     runtime "Debug"
     symbols "on"

  filter "configurations:Release"
    runtime "Release"
    optimize "on"

  newoption{
    trigger = "examples",
    description = "Generates examples"
  }
if(_OPTIONS["examples"]) then 
  project "JsonParserExample"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ("Build/Bin/%{prj.name}/%{cfg.longname}")
    objdir ("Build/Obj/%{prj.name}/%{cfg.longname}")
     
    files {        "examples/example1.cpp"  }

    includedirs "include/"
    links "JsonParserLib"

    filter "configurations:Debug"
      runtime "Debug"
      symbols "on"

    filter "configurations:Release"
      runtime "Release"
      optimize "on"
end
