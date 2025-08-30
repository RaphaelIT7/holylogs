workspace("HolyLogs")
	configurations({"Debug", "Release"})

project("HolyLogs")
	kind("ConsoleApp")
	language("C++")
	targetdir("bin/%{cfg.buildcfg}")
	cppdialect("C++26")

	defines("CPPHTTPLIB_NO_EXCEPTIONS")

	files({
		"source/*.cpp",
		"source/*.h",
		"source/routes/*.cpp",
		"source/routes/*.h",
		"README.md",
	})

	includedirs("source/")

	vpaths({
		["Routes"] = "source/routes/*",
		["Source Files"] = "source/*",
	})

	filter("configurations:Debug")
	  defines({"DEBUG"})
	  symbols("On")

   filter("configurations:Release")
	  defines({"NDEBUG"})
	  optimize("On")