import pynopegl as ngl


@ngl.scene()
def circle(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (4, 3)
    shape = ngl.ShapeCircle(radius=0.7)
    return ngl.DrawColor(shape=shape, blending="src_over")
