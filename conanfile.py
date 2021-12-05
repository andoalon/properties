import conans

class PropertiesConan(conans.ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    requires = (
        "rapidjson/1.1.0",
        "yaml-cpp/0.7.0",
    )

    options = {
        #"shared": [True, False], # No DLL support
        "with_imgui" : [True, False],
    }

    default_options = {
        "with_imgui" : False,

        "yaml-cpp:shared" : False,

        "imgui-sfml:shared" : False,

        "sfml:shared" : False,
        "sfml:audio": False,
        "sfml:graphics": True,
        "sfml:network": False,
        "sfml:window": True,
    }

    def requirements(self):
        if self.options.with_imgui:
            self.requires("imgui-sfml/2.3@bincrafters/stable")

            # The version should ideally match the one used by imgui-sfml
            self.requires("sfml/2.5.1")

