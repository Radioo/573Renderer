#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace AfpScript::Detail {

struct NameRun {
    uint16_t first;
    std::string_view names;
};

constexpr std::array<NameRun, 33> kNameRuns{{
    {.first = 0x100,
     .names = "_x _y _xscale _yscale _currentframe _totalframes _alpha _visible _width _height"
              " _rotation _target _framesloaded _name _droptarget _url _highquality _focusrect"
              " _soundbuftime _quality _xmouse _ymouse _z"},
    {.first = 0x120, .names = "this _root _parent _global arguments"},
    {.first = 0x140,
     .names = "blendMode enabled hitArea _lockroot $version numChildren transform graphics"
              " loaderInfo mask upState overState downState hitTestState doubleClickEnabled"
              " cacheAsBitmap scrollRect opaqueBackground tabChildren tabEnabled tabIndex"
              " mouseEnabled mouseChildren buttonMode useHandCursor"},
    {.first = 0x160,
     .names = "textWidth textHeight text autoSize textColor selectable multiline wordWrap border"
              " borderColor background backgroundColor embedFonts defaultTextFormat htmlText"
              " mouseWheelEnabled maxChars sharpness thickness antiAliasType gridFitType maxScrollH"
              " maxScrollV restrict numLines"},
    {.first = 0x180, .names = "ra rb ga gb ba bb aa ab"},
    {.first = 0x1a0,
     .names = "font size color bold italic underline url target align leftMargin rightMargin indent"
              " leading letterSpacing"},
    {.first = 0x1c0,
     .names = "a b c d e f g h i j k l m n o p q r s t u v w x y z tx ty length ignoreWhite loaded"
              " childNodes firstChild nodeValue nextSibling nodeName nodeType attributes __count"
              " __type width height useCodepage duration position matrixType matrix prototype"
              " __proto__ xMin xMax yMin yMax lastChild parentNode previousSibling callee caller"
              " colorTransform concatenatedColorTransform concatenatedMatrix pixelBounds matrix3D"
              " perspectiveProjection FSCommand:fullscreen FSCommand:showmenu FSCommand:allowscale"
              " FSCommand:quit NaN Infinity number boolean string object movieclip null undefined"
              " function normal layer darken multiply lighten screen overlay hardlight subtract"
              " difference invert alpha erase / .. linear radial none square miter bevel left right"
              " center box reflect repeat RGB linearRGB justify shader vertical horizontal pad"
              " evenOdd nonZero negative positive xml B BL BR L R T TL TR exactFit noBorder noScale"
              " showAll easeInSine easeOutSine easeInOutSine easeOutInSine easeInQuad easeOutQuad"
              " easeInOutQuad easeOutInQuad easeInFlash easeOutFlash element dynamic binary"
              " variables LB RB LT RT"},
    {.first = 0x254,
     .names =
         "arrow auto button hand ibeam advanced pixel subpixel full inner outer easeInBack"
         " easeOutBack easeInOutBack easeOutInBack registerClassConstructor setter getter ???"
         " aep_dummy kind _kind org flashdevelop utils FlashConnect path if notif not A dmy0273"
         " C D dmy0276 F G H I J K dmy027d M N O P Q dmy0283 S dmy0285 U V W X Y Z fullscreen"
         " showmenu allowscale quit true false clamp ignore wrap unknown bigEndian littleEndian"
         " fragment vertex bgra bgraPacked4444 bgrPacked565 compressed compressedAlpha bytes4"
         " float1 float2 float3 float4 super axisAngle eulerAngles quaternion orientationStyle"},
    {.first = 0x2e0,
     .names = "_level0 _level1 _level2 _level3 _level4 _level5 _level6 _level7 _level8 _level9"
              " _level10 _level11 _level12 _level13 _level14 _level15"},
    {.first = 0x300,
     .names =
         "System Stage Key Math flash MovieClip String TextField Color Date SharedObject Mouse"
         " Object Sound Number Array XML TextFormat display geom Matrix Point BitmapData data"
         " filters ColorMatrixFilter Function XMLNode aplib Transform ColorTransform Rectangle"
         " asdlib XMLController eManager Error MovieClipLoader UndefChecker int uint Vector"
         " Event MouseEvent Matrix3D Keyboard DisplayObject Dictionary BlendMode"
         " DisplayObjectContainer Class EventDispatcher PerspectiveProjection Vector3D aplib3"
         " SoundChannel Loader URLRequest Sprite KeyboardEvent Timer TimerEvent asdlib3"
         " eManager3 LoaderInfo ProgressEvent IOErrorEvent Graphics LineScaleMode CapsStyle"
         " JointStyle GradientType SpreadMethod InterpolationMethod GraphicsPathCommand"
         " GraphicsPathWinding TriangleCulling GraphicsBitmapFill GraphicsEndFill"
         " GraphicsGradientFill GraphicsPath GraphicsSolidFill GraphicsStroke"
         " GraphicsTrianglePath IGraphicsData external ExternalInterface Scene FrameLabel Shape"
         " SimpleButton Bitmap StageQuality InteractiveObject MotionBase KeyframeBase XMLList"
         " StageAlign StageScaleMode AnimatorBase Animator3D URLLoader Capabilities Aweener"
         " Aweener3 SoundTransform Namespace RegExp afplib afplib3 ByteArray TextFormatAlign"
         " TextFieldType TextFieldAutoSize SecurityErrorEvent ApplicationDomain TextEvent"
         " ErrorEvent LoaderContext QName IllegalOperationError URLLoaderDataFormat Security"
         " DropShadowFilter ReferenceError Proxy XMLSocket DataEvent Font IEventDispatcher"
         " LocalConnection ActionScriptVersion MouseCursor TypeError FocusEvent AntiAliasType"
         " GridFitType ArgumentError BitmapFilterType BevelFilter BitmapFilter"
         " BitmapFilterQuality XMLController3 URLVariables URLRequestMethod aeplib BlurFilter"
         " Stage3D Context3D Multitouch Script AccessibilityProperties StaticText MorphShape"
         " BitmapDataChannel DisplacementMapFilter GlowFilter DisplacementMapFilterMode"
         " AnimatorFactoryBase Endian IOError EOFError Context3DTextureFormat"
         " Context3DProgramType TextureBase VertexBuffer3D IndexBuffer3D Program3D"
         " NativeMenuItem ContextMenuItem NativeMenu ContextMenu ContextMenuEvent"
         " Context3DVertexBufferFormat TouchEvent b2Vec2 b2Math b2Transform b2Mat22 b2Sweep"
         " b2AABB b2Vec3 b2Mat33 b2DistanceProxy b2Shape b2CircleShape b2PolygonShape"
         " b2MassData b2DistanceInput b2DistanceOutput b2SimplexCache b2Simplex b2SimplexVertex"
         " b2Distance Orientation3D GradientGlowFilter GradientBevelFilter"},
    {.first = 0x400,
     .names =
         "afp_prop_init afp_prop afp_prop_dummy afp_prop_destroy afp_sync afp_node_search"
         " afp_node_value afp_node_array_value afp_complete afp_sound_fade_in"
         " afp_sound_fade_out afp_make_gradient_data afp_make_alpha_texture afp_node_set_value"
         " afp_node_date_value afp_node_num_value afp_node_array_num_value afp_node_child"
         " afp_node_parent afp_node_next afp_node_prev afp_node_last afp_node_first"
         " afp_node_next_same_name afp_node_prev_same_name afp_node_name afp_node_absolute_path"
         " afp_node_has_parent afp_node_has_child afp_node_has_sibling afp_node_has_attrib"
         " afp_node_has_same_name_sibling updateAfterEvent parseInt parseFloat Boolean"
         " setInterval clearInterval escape ASSetPropFlags unescape isNaN isFinite trace"
         " addFrameScript getDefinitionByName getTimer setTimeout clearTimeout escapeMultiByte"
         " unescapeMultiByte getQualifiedClassName describeType decodeURI encodeURI"
         " decodeURIComponent encodeURIComponent registerClassAlias getClassByAlias"
         " getQualifiedSuperclassName isXMLName fscommand"},
    {.first = 0x440,
     .names = "stop play gotoAndPlay gotoAndStop prevFrame nextFrame createEmptyMovieClip"
              " duplicateMovieClip attachMovie attachBitmap removeMovieClip unloadMovie loadMovie"
              " loadVariables startDrag stopDrag setMask hitTest lineStyle lineGradientStyle"
              " beginFill beginBitmapFill endFill moveTo lineTo curveTo clear getBytesLoaded"
              " getBytesTotal getDepth getNextHighestDepth swapDepths localToGlobal"
              " beginGradientFill getSWFVersion getRect getBounds getInstanceAtDepth getURL"
              " globalToLocal nextScene prevScene getChildByName getChildIndex addChild"
              " removeChildAt getChildAt setChildIndex lineBitmapStyle hitTestObject hitTestPoint"
              " addChildAt removeChild swapChildren swapChildrenAt getObjectsUnderPoint"
              " createTextField local3DToGlobal globalToLocal3D"},
    {.first = 0x480,
     .names = "toString distance translate rotate scale clone transformPoint add cos sin sqrt atan2"
              " log abs floor ceil round pow max min random acos asin atan tan exp getRGB setRGB"
              " getTransform setTransform fromCharCode substr substring toUpperCase toLowerCase"
              " indexOf lastIndexOf charAt charCodeAt split concat getFullYear getUTCFullYear"
              " getMonth getUTCMonth getDate getUTCDate getDay getHours getUTCHours getMinutes"
              " getUTCMinutes getSeconds getUTCSeconds getTime getTimezoneOffset UTC createElement"
              " appendChild createTextNode parseXML load hasChildNodes cloneNode removeNode"
              " loadInAdvance createGradientBox loadBitmap hide show addListener removeListener"
              " isDown getCode getAscii attachSound start getVolume setVolume setPan loadSound"
              " setTextFormat getTextFormat push pop slice splice reverse sort flush getLocal shift"
              " unshift registerClass getUTCDay getMilliseconds getUTCMilliseconds addProperty"
              " hasOwnProperty isPropertyEnumerable isPrototypeOf unwatch valueOf watch apply call"
              " contains containsPoint containsRectangle equals inflate inflatePoint intersection"
              " intersects isEmpty offset offsetPoint setEmpty union interpolate join loadClip"
              " getProgress unloadClip polar sortOn containsRect getYear onKeyDown onKeyUp"
              " onMouseDown onMouseUp onMouseMove onLoad onEnterFrame onUnload onRollOver onRollOut"
              " onPress onRelease onReleaseOutside onData onSoundComplete onDragOver onDragOut"
              " onMouseWheel onLoadError onLoadComplete onLoadInit onLoadProgress onLoadStart"
              " onComplete onCompleteParams ononCompleteScope onStart onStartParams onStartScope"
              " onUpdate onUpdateParams onUpdateScope onKeyPress onInitialize onConstruct"},
    {.first = 0x5c0,
     .names = "scaleX scaleY currentFrame totalFrames visible rotation framesLoaded dropTarget"
              " focusRect mouseX mouseY root parent stage currentLabel currentLabels"
              " currentFrameLabel currentScene scenes rotationX rotationY rotationZ quality skewX"
              " skewY rotationConcat useRotationConcat scaleZ isPlaying"},
    {.first = 0x600,
     .names = "BACKSPACE CAPSLOCK CONTROL DELETEKEY DOWN END ENTER ESCAPE HOME INSERT LEFT PGDN"
              " PGUP RIGHT SHIFT SPACE TAB UP ARROW AUTO BUTTON HAND IBEAM"},
    {.first = 0x620, .names = "CASEINSENSITIVE DESCENDING UNIQUESORT RETURNINDEXEDARRAY NUMERIC"},
    {.first = 0x640,
     .names = "ADD ALPHA DARKEN DIFFERENCE ERASE HARDLIGHT INVERT LAYER LIGHTEN MULTIPLY NORMAL"
              " OVERLAY SCREEN SHADER SUBTRACT"},
    {.first = 0x660,
     .names = "dmy0660 NONE VERTICAL HORIZONTAL ROUND SQUARE BEVEL MITER LINEAR RADIAL PAD REFLECT"
              " REPEAT LINEAR_RGB NO_OP MOVE_TO LINE_TO CURVE_TO WIDE_MOVE_TO WIDE_LINE_TO EVEN_ODD"
              " NON_ZERO NEGATIVE POSITIVE FRAGMENT VERTEX BGRA BGRA_PACKED BGR_PACKED COMPRESSED"
              " COMPRESSED_ALPHA BYTES_4 FLOAT_1 FLOAT_2 FLOAT_3 FLOAT_4 e_unknownShape"
              " e_circleShape e_polygonShape e_edgeShape e_shapeTypeCount CUBIC_CURVE_TO"},
    {.first = 0x690,
     .names = "BOTTOM BOTTOM_LEFT BOTTOM_RIGHT TOP TOP_LEFT TOP_RIGHT EXACT_FIT NO_BORDER NO_SCALE"
              " SHOW_ALL"},
    {.first = 0x6a0,
     .names = "CENTER JUSTIFY dmy06a2 dmy06a3 dmy06a4 dmy06a5 dmy06a6 dmy06a7 dmy06a8 DYNAMIC INPUT"
              " ADVANCED PIXEL SUBPIXEL"},
    {.first = 0x6b0, .names = "BINARY TEXT VARIABLES"},
    {.first = 0x6c0, .names = "FULL INNER OUTER RED GREEN BLUE CLAMP COLOR IGNORE WRAP"},
    {.first = 0x6d0,
     .names = "DELETE GET HEAD OPTIONS POST PUT BIG_ENDIAN LITTLE_ENDIAN AXIS_ANGLE EULER_ANGLES"
              " QUATERNION"},
    {.first = 0x6f0,
     .names = "NUMBER_0 NUMBER_1 NUMBER_2 NUMBER_3 NUMBER_4 NUMBER_5 NUMBER_6 NUMBER_7 NUMBER_8"
              " NUMBER_9"},
    {.first = 0x700,
     .names =
         "redMultiplier greenMultiplier blueMultiplier alphaMultiplier redOffset greenOffset"
         " blueOffset alphaOffset rgb bottom bottomRight top topLeft LOW MEDIUM HIGH BEST name"
         " message bytesLoaded bytesTotal once MAX_VALUE MIN_VALUE NEGATIVE_INFINITY"
         " POSITIVE_INFINITY stageWidth stageHeight frame numFrames labels currentTarget void"
         " fixed rawData type focalLength fieldOfView projectionCenter E LN10 LN2 LOG10E LOG2E"
         " PI SQRT1_2 SQRT2 stageX stageY localX localY tintColor tintMultiplier brightness"
         " delay repeatCount currentCount running charCode keyCode altKey ctrlKey shiftKey"
         " useCodePage contentLoaderInfo loaderURL loader fullYear fullYearUTC month monthUTC"
         " date dateUTC day dayUTC hours hoursUTC minutes minutesUTC seconds secondsUTC"
         " milliseconds millisecondsUTC timezoneOffset time joints fill colors commands"
         " miterLimit alphas ratios bitmapData vertices uvtData indices parameters frameRate"
         " low medium high best index blank is3D scaleMode frameEvent motion"
         " transformationPoint transformationPointZ sceneName targetParent targetName"
         " initialPosition children child playerType os capabilities transition"
         " transitionParameters useFrames _color_redMultiplier _color_redOffset"
         " _color_greenMultiplier _color_greenOffset _color_blueMultiplier _color_blueOffset"
         " _color_alphaMultiplier _color_alphaOffset _color soundTransform volume uri prefix"
         " content contentType swfVersion input source lastIndex stageFocusRect currentDomain"
         " applicationDomain parentDomain dataFormat digest errorID available client"
         " actionScriptVersion ACTIONSCRIPT2 ACTIONSCRIPT3 delta cursor buttonDown motionArray"
         " spanStart spanEnd placeholderName instanceFactoryClass instance method"
         " requestHeaders info dotall extended global ignoreCase X_AXIS Y_AXIS Z_AXIS"
         " lengthSquared dps from0 _text _text_sound blurX blurY step spc stage3Ds context3D"
         " version accessibilityProperties description adjustColorBrightness"
         " adjustColorContrast adjustColorSaturation adjustColorHue strength angle knockout"
         " hideObject manufacturer smoothing rect transparent mapBitmap mapPoint componentX"
         " componentY mode highlightColor highlightAlpha shadowColor shadowAlpha endian _scale"
         " transitionParams _text_color _text_color_r _text_color_g _text_color_b cancelable"
         " col1 col2 localCenter t0 a0 c0 lowerBound upperBound col3 mass m_vertices"
         " m_vertexCount m_radius m_p m_normals m_type m_centroid proxyA proxyB transformA"
         " transformB useRadii pointA pointB iterations count metric indexA indexB determinant"},
    {.first = 0x800,
     .names =
         "sound_play sound_stop sound_stop_all set_top_mc set_controlled_XML set_config_XML"
         " set_data_top attach_event detach_event set get ready afp_available"
         " controller_available push_state pop_state afp_verbose_action sound_fadeout"
         " sound_fadeout_all deepPlay deepStop deepGotoAndPlay deepGotoAndStop detach_event_all"
         " detach_event_id detach_event_obj get_version get_version_str addAween addCaller"
         " registerSpecialProperty registerSpecialPropertySplitter registerTransition"
         " removeAllAweens removeAweens use_konami_lib sound_volume sound_volume_all"
         " afp_verbose_script afp_node_check_value afp_node_check get_version_full_str"
         " set_debug num2str_comma areacode2str num2str_period get_columns num2mc warning fatal"
         " aep_set_frame_control aep_set_rect_mask load_movie get_movie_clip aep_set_set_frame"
         " deep_goto_play_label deep_goto_stop_label goto_play_label goto_stop_label goto_play"
         " goto_stop set_text get_text_data attach_movie attach_bitmap create_movie_clip"
         " set_text_scroll set_stage"},
    {.first = 0x880,
     .names = "flash.system flash.display flash.text fl.motion flash.net flash.ui flash.geom"
              " flash.filters flash.events flash.utils flash.media flash.external flash.errors"
              " flash.xml flash.display3D flash.accessibility flash.display3D.textures"
              " Box2D.Common.Math Box2D.Collision Box2D.Collision.Shapes"},
    {.first = 0x900,
     .names =
         "setDate setUTCDate setFullYear setUTCFullYear setHours setUTCHours setMilliseconds"
         " setUTCMilliseconds setMinutes setUTCMinutes setMonth setUTCMonth setSeconds"
         " setUTCSeconds setTime setYear addEventListener removeEventListener match replace"
         " search append appendRotation appendScale appendTranslation decompose"
         " deltaTransformVector identity interpolateTo pointAt prepend prependRotation"
         " prependScale prependTranslation recompose transformVector transformVectors transpose"
         " dispatchEvent toMatrix3D appendText getLineText replaceText propertyIsEnumerable"
         " setPropertyIsEnumerable drawCircle drawEllipse drawGraphicsData drawPath drawRect"
         " drawRoundRect drawTriangles copyFrom addCallback overrideTargetTransform"
         " addPropertyArray getCurrentKeyframe getMatrix3D getColorTransform getFilters"
         " getValue hasEventListener registerParentFrameHandler processCurrentFrame normalize"
         " elements toXMLString attribute localName nodeKind exec toLocaleString invalidate"
         " getDefinition hasDefinition descendants loadPolicyFile reset callProperty"
         " getProperty setProperty willTrigger send addTargetInfo drawRoundRectComplex forEach"
         " filter every some map test toDateString toLocaleDateString toLocaleTimeString"
         " toTimeString toUTCString parse project nearEquals scaleBy negate incrementBy"
         " decrementBy dotProduct crossProduct angleBetween decode copyColumnFrom copyColumnTo"
         " copyRawDataFrom copyRawDataTo copyRowFrom copyRowTo copyToMatrix3D requestContext3D"
         " initFilters addFilterPropertyArray setTo createBox deltaTransformPoint writeByte"
         " writeInt readByte writeBoolean writeDouble readBoolean readDouble writeUnsignedInt"
         " getVector setVector unload unloadAndStop toExponential toFixed toPrecision"
         " getNewTextFormat SetV Set SetZero Make Copy Length LengthSquared Normalize Multiply"
         " GetNegative NegativeSelf Clamp MulX Dot SubtractVV CrossVF CrossFV MulTMV CrossVV"
         " Max MulMV Abs MulXT SetM AddM Solve Add Min FromAngle GetTransform Advance"
         " GetInverse Combine Contains TestOverlap GetCenter Solve22 Solve33 SetLocalPosition"
         " ComputeAABB ComputeMass GetType SetAsArray GetVertex GetSupport GetSupportVertex"
         " GetVertexCount GetVertices ReadCache GetClosestPoint GetSearchDirection Solve2"
         " Solve3 GetWitnessPoints WriteCache Distance cubicCurveTo wideMoveTo wideLineTo"
         " insertAt removeAt"},
    {.first = 0xa00,
     .names = "CLICK ENTER_FRAME ADDED_TO_STAGE MOUSE_DOWN MOUSE_MOVE MOUSE_OUT MOUSE_OVER MOUSE_UP"
              " MOUSE_WHEEL ROLL_OUT ROLL_OVER KEY_DOWN KEY_UP TIMER COMPLETE SOUND_COMPLETE OPEN"
              " PROGRESS INIT IO_ERROR TIMER_COMPLETE REMOVED_FROM_STAGE REMOVED FRAME_CONSTRUCTED"
              " DOUBLE_CLICK RESIZE ADDED TAB_CHILDREN_CHANGE TAB_ENABLED_CHANGE TAB_INDEX_CHANGE"
              " EXIT_FRAME RENDER ACTIVATE DEACTIVATE SECURITY_ERROR ERROR CLOSE DATA CONNECT"
              " MOUSE_LEAVE FOCUS_IN FOCUS_OUT KEY_FOCUS_CHANGE MOUSE_FOCUS_CHANGE LINK TEXT_INPUT"
              " CHANGE SCROLL CONTEXT3D_CREATE MENU_ITEM_SELECT MENU_SELECT UNLOAD TOUCH_BEGIN"
              " TOUCH_END TOUCH_MOVE TOUCH_OUT TOUCH_OVER TOUCH_ROLL_OUT TOUCH_ROLL_OVER TOUCH_TAP"
              " CONTEXT_MENU MIDDLE_CLICK MIDDLE_MOUSE_DOWN MIDDLE_MOUSE_UP RELEASE_OUTSIDE"
              " RIGHT_CLICK RIGHT_MOUSE_DOWN RIGHT_MOUSE_UP"},
    {.first = 0xa80,
     .names =
         "click enterFrame addedToStage mouseDown mouseMove mouseOut mouseOver mouseUp"
         " mouseWheel rollOut rollOver keyDown keyUp timer complete soundComplete open progress"
         " init ioError timerComplete removedFromStage removed frameConstructed doubleClick"
         " resize added tabChildrenChange tabEnabledChange tabIndexChange exitFrame render"
         " activate deactivate securityError error close udf0aa5 connect mouseLeave focusIn"
         " focusOut keyFocusChange mouseFocusChange link textInput change scroll"
         " context3DCreate menuItemSelect menuSelect udf0ab3 touchBegin touchEnd touchMove"
         " touchOut touchOver touchRollOut touchRollOver touchTap contextMenu middleClick"
         " middleMouseDown middleMouseUp releaseOutside rightClick rightMouseDown rightMouseUp"},
    {.first = 0xb00,
     .names =
         "flash.system.System flash.display.Stage udf0b02 sme0b03 udf0b04"
         " flash.display.MovieClip sme0b06 flash.text.TextField fl.motion.Color sme0b09"
         " flash.net.SharedObject flash.ui.Mouse sme0b0c flash.media.Sound sme0b0e sme0b0f"
         " sme0b10 flash.text.TextFormat udf0b12 udf0b13 flash.geom.Matrix flash.geom.Point"
         " flash.display.BitmapData udf0b17 udf0b18 flash.filters.ColorMatrixFilter sme0b1a"
         " flash.xml.XMLNode sme0b1c flash.geom.Transform flash.geom.ColorTransform"
         " flash.geom.Rectangle sme0b20 sme0b21 sme0b22 sme0b23 udf0b24 sme0b25 sme0b26 sme0b27"
         " sme0b28 flash.events.Event flash.events.MouseEvent flash.geom.Matrix3D"
         " flash.ui.Keyboard flash.display.DisplayObject flash.utils.Dictionary"
         " flash.display.BlendMode flash.display.DisplayObjectContainer sme0b31"
         " flash.events.EventDispatcher flash.geom.PerspectiveProjection flash.geom.Vector3D"
         " sme0b35 flash.media.SoundChannel flash.display.Loader flash.net.URLRequest"
         " flash.display.Sprite flash.events.KeyboardEvent flash.utils.Timer"
         " flash.events.TimerEvent sme0b3d sme0b3e flash.display.LoaderInfo"
         " flash.events.ProgressEvent flash.events.IOErrorEvent flash.display.Graphics"
         " flash.display.LineScaleMode flash.display.CapsStyle flash.display.JointStyle"
         " flash.display.GradientType flash.display.SpreadMethod"
         " flash.display.InterpolationMethod flash.display.GraphicsPathCommand"
         " flash.display.GraphicsPathWinding flash.display.TriangleCulling"
         " flash.display.GraphicsBitmapFill flash.display.GraphicsEndFill"
         " flash.display.GraphicsGradientFill flash.display.GraphicsPath"
         " flash.display.GraphicsSolidFill flash.display.GraphicsStroke"
         " flash.display.GraphicsTrianglePath flash.display.IGraphicsData udf0b54"
         " flash.external.ExternalInterface flash.display.Scene flash.display.FrameLabel"
         " flash.display.Shape flash.display.SimpleButton flash.display.Bitmap"
         " flash.display.StageQuality flash.display.InteractiveObject fl.motion.MotionBase"
         " fl.motion.KeyframeBase sme0b5f flash.display.StageAlign flash.display.StageScaleMode"
         " fl.motion.AnimatorBase fl.motion.Animator3D flash.net.URLLoader"
         " flash.system.Capabilities sme0b66 sme0b67 flash.media.SoundTransform sme0b69 sme0b6a"
         " sme0b6b sme0b6c flash.utils.ByteArray flash.text.TextFormatAlign"
         " flash.text.TextFieldType flash.text.TextFieldAutoSize"
         " flash.events.SecurityErrorEvent flash.system.ApplicationDomain"
         " flash.events.TextEvent flash.events.ErrorEvent flash.system.LoaderContext sme0b76"
         " flash.errors.IllegalOperationError flash.net.URLLoaderDataFormat"
         " flash.system.Security flash.filters.DropShadowFilter sme0b7b flash.utils.Proxy"
         " flash.net.XMLSocket flash.events.DataEvent flash.text.Font"
         " flash.events.IEventDispatcher flash.net.LocalConnection"
         " flash.display.ActionScriptVersion flash.ui.MouseCursor sme0b84"
         " flash.events.FocusEvent flash.text.AntiAliasType flash.text.GridFitType sme0b88"
         " flash.filters.BitmapFilterType flash.filters.BevelFilter flash.filters.BitmapFilter"
         " flash.filters.BitmapFilterQuality sme0b8d flash.net.URLVariables"
         " flash.net.URLRequestMethod sme0b90 flash.filters.BlurFilter flash.display.Stage3D"
         " flash.display3D.Context3D flash.ui.Multitouch udf0b95"
         " flash.accessibility.AccessibilityProperties flash.text.StaticText"
         " flash.display.MorphShape flash.display.BitmapDataChannel"
         " flash.filters.DisplacementMapFilter flash.filters.GlowFilter"
         " flash.filters.DisplacementMapFilterMode fl.motion.AnimatorFactoryBase"
         " flash.utils.Endian flash.errors.IOError flash.errors.EOFError"
         " flash.display3D.Context3DTextureFormat flash.display3D.Context3DProgramType"
         " flash.display3D.textures.TextureBase flash.display3D.VertexBuffer3D"
         " flash.display3D.IndexBuffer3D flash.display3D.Program3D flash.display.NativeMenuItem"
         " flash.ui.ContextMenuItem flash.display.NativeMenu flash.ui.ContextMenu"
         " flash.events.ContextMenuEvent flash.display3D.Context3DVertexBufferFormat"
         " flash.events.TouchEvent Box2D.Common.Math.b2Vec2 Box2D.Common.Math.b2Math"
         " Box2D.Common.Math.b2Transform Box2D.Common.Math.b2Mat22 Box2D.Common.Math.b2Sweep"
         " Box2D.Collision.b2AABB Box2D.Common.Math.b2Vec3 Box2D.Common.Math.b2Mat33"
         " Box2D.Collision.b2DistanceProxy Box2D.Collision.Shapes.b2Shape"
         " Box2D.Collision.Shapes.b2CircleShape Box2D.Collision.Shapes.b2PolygonShape"
         " Box2D.Collision.Shapes.b2MassData Box2D.Collision.b2DistanceInput"
         " Box2D.Collision.b2DistanceOutput Box2D.Collision.b2SimplexCache"
         " Box2D.Collision.b2Simplex Box2D.Collision.b2SimplexVertex Box2D.Collision.b2Distance"
         " flash.geom.Orientation3D flash.filters.GradientGlowFilter"
         " flash.filters.GradientBevelFilter"},
    {.first = 0xc00, .names = "NEARLY_ZERO EXACTLY_ZERO debug_mode"},
    {.first = 0xd00,
     .names = "m_count wA wB bubbles checkPolicyFile securityDomain spreadMethod"
              " interpolationMethod focalPointRatio culling caps winding"},
}};

}
