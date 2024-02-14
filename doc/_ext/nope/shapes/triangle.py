import pynopegl as ngl


@ngl.scene()
def triangle(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (4, 3)
    shape = ngl.ShapeTriangle(radius=0.7, layout="fit")
    return ngl.DrawColor(shape=shape, blending="src_over")
