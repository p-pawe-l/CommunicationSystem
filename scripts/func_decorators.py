def generating_func(fn):
    """Decorator for function marked as data generator"""
    fn._client_func_type = "generate"
    return fn

def processing_func(fn):
    """Decorator for function marked as data processor"""
    fn._client_func_type = "process"
    return fn
