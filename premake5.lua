
project "JsonParserLib"
   kind "StaticLib"
   language "C++"
   
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

project "JsonParserExample"
   kind "ConsoleApp"
   language "C++"
   staticruntime "off"

   targetdir ("Build/Bin/%{prj.name}/%{cfg.longname}")
   objdir ("Build/Obj/%{prj.name}/%{cfg.longname}")
   
   files {        "example/main.cpp"  }

   includedirs "include/"
   links "JsonParserLib"
   
   filter "configurations:Debug"
      defines { "DEBUG" }
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
