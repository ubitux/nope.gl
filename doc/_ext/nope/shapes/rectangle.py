import pynopegl as ngl


@ngl.scene()
def rectangle(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (4, 3)
    shape = ngl.ShapeRectangle(rounding=(0.0, 0.6, 0.0, 0.3))
    return ngl.DrawColor(shape=shape, blending="src_over")
