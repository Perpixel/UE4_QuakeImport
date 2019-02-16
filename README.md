# Quake BSP import for Unreal4

Import bsp right into your content. A pak extractor is provided (pakutil.exe) and the palette.lmp file need to be located in this plugin content folder in order for the texture to import.

On import several files will be created.

/game/materials/... // All materials for each textures

/game/textures/... // All textures

/game/maps/... // level with entities

/game/models/name_of_map/... // all submodels. 0 is the main mesh and everything else numbered are for entities like door and platforms


This project require manual assembly and I do not provide any of the game code/ blueprint classes to put it all together. Plugin to import alias (.mdl) to come later.
