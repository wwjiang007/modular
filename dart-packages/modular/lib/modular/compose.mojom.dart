// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// WARNING: DO NOT EDIT. This file was generated by a program.
// See $MOJO_SDK/tools/bindings/mojom_bindings_generator.py.

library compose_mojom;
import 'dart:async';
import 'package:mojo/bindings.dart' as bindings;
import 'package:mojo/core.dart' as core;
import 'package:mojo/mojo/bindings/types/service_describer.mojom.dart' as service_describer;
import 'package:mojo_services/mojo/ui/view_token.mojom.dart' as view_token_mojom;



class ModuleInstanceDisplayData extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(48, 0)
  ];
  String url = null;
  view_token_mojom.ViewOwnerInterface viewOwner = null;
  ComposableInterface childInterface = null;
  List<String> embodiments = null;
  String liveSuggestionId = null;

  ModuleInstanceDisplayData() : super(kVersions.last.size);

  ModuleInstanceDisplayData.init(
    String this.url, 
    view_token_mojom.ViewOwnerInterface this.viewOwner, 
    ComposableInterface this.childInterface, 
    List<String> this.embodiments, 
    String this.liveSuggestionId
  ) : super(kVersions.last.size);

  static ModuleInstanceDisplayData deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static ModuleInstanceDisplayData decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    ModuleInstanceDisplayData result = new ModuleInstanceDisplayData();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.url = decoder0.decodeString(8, false);
    }
    if (mainDataHeader.version >= 0) {
      
      result.viewOwner = decoder0.decodeServiceInterface(16, false, view_token_mojom.ViewOwnerProxy.newFromEndpoint);
    }
    if (mainDataHeader.version >= 0) {
      
      result.childInterface = decoder0.decodeServiceInterface(24, false, ComposableProxy.newFromEndpoint);
    }
    if (mainDataHeader.version >= 0) {
      
      var decoder1 = decoder0.decodePointer(32, false);
      {
        var si1 = decoder1.decodeDataHeaderForPointerArray(bindings.kUnspecifiedArrayLength);
        result.embodiments = new List<String>(si1.numElements);
        for (int i1 = 0; i1 < si1.numElements; ++i1) {
          
          result.embodiments[i1] = decoder1.decodeString(bindings.ArrayDataHeader.kHeaderSize + bindings.kPointerSize * i1, false);
        }
      }
    }
    if (mainDataHeader.version >= 0) {
      
      result.liveSuggestionId = decoder0.decodeString(40, true);
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    try {
      encoder0.encodeString(url, 8, false);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "url of struct ModuleInstanceDisplayData: $e";
      rethrow;
    }
    try {
      encoder0.encodeInterface(viewOwner, 16, false);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "viewOwner of struct ModuleInstanceDisplayData: $e";
      rethrow;
    }
    try {
      encoder0.encodeInterface(childInterface, 24, false);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "childInterface of struct ModuleInstanceDisplayData: $e";
      rethrow;
    }
    try {
      if (embodiments == null) {
        encoder0.encodeNullPointer(32, false);
      } else {
        var encoder1 = encoder0.encodePointerArray(embodiments.length, 32, bindings.kUnspecifiedArrayLength);
        for (int i0 = 0; i0 < embodiments.length; ++i0) {
          encoder1.encodeString(embodiments[i0], bindings.ArrayDataHeader.kHeaderSize + bindings.kPointerSize * i0, false);
        }
      }
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "embodiments of struct ModuleInstanceDisplayData: $e";
      rethrow;
    }
    try {
      encoder0.encodeString(liveSuggestionId, 40, true);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "liveSuggestionId of struct ModuleInstanceDisplayData: $e";
      rethrow;
    }
  }

  String toString() {
    return "ModuleInstanceDisplayData("
           "url: $url" ", "
           "viewOwner: $viewOwner" ", "
           "childInterface: $childInterface" ", "
           "embodiments: $embodiments" ", "
           "liveSuggestionId: $liveSuggestionId" ")";
  }

  Map toJson() {
    throw new bindings.MojoCodecError(
        'Object containing handles cannot be encoded to JSON.');
  }
}


class _ComposerAddChildParams extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(24, 0)
  ];
  String id = null;
  ModuleInstanceDisplayData moduleData = null;

  _ComposerAddChildParams() : super(kVersions.last.size);

  _ComposerAddChildParams.init(
    String this.id, 
    ModuleInstanceDisplayData this.moduleData
  ) : super(kVersions.last.size);

  static _ComposerAddChildParams deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static _ComposerAddChildParams decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    _ComposerAddChildParams result = new _ComposerAddChildParams();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.id = decoder0.decodeString(8, false);
    }
    if (mainDataHeader.version >= 0) {
      
      var decoder1 = decoder0.decodePointer(16, false);
      result.moduleData = ModuleInstanceDisplayData.decode(decoder1);
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    try {
      encoder0.encodeString(id, 8, false);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "id of struct _ComposerAddChildParams: $e";
      rethrow;
    }
    try {
      encoder0.encodeStruct(moduleData, 16, false);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "moduleData of struct _ComposerAddChildParams: $e";
      rethrow;
    }
  }

  String toString() {
    return "_ComposerAddChildParams("
           "id: $id" ", "
           "moduleData: $moduleData" ")";
  }

  Map toJson() {
    throw new bindings.MojoCodecError(
        'Object containing handles cannot be encoded to JSON.');
  }
}


class _ComposerRemoveChildParams extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(16, 0)
  ];
  String id = null;

  _ComposerRemoveChildParams() : super(kVersions.last.size);

  _ComposerRemoveChildParams.init(
    String this.id
  ) : super(kVersions.last.size);

  static _ComposerRemoveChildParams deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static _ComposerRemoveChildParams decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    _ComposerRemoveChildParams result = new _ComposerRemoveChildParams();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.id = decoder0.decodeString(8, false);
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    try {
      encoder0.encodeString(id, 8, false);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "id of struct _ComposerRemoveChildParams: $e";
      rethrow;
    }
  }

  String toString() {
    return "_ComposerRemoveChildParams("
           "id: $id" ")";
  }

  Map toJson() {
    Map map = new Map();
    map["id"] = id;
    return map;
  }
}


class _ComposerUpdateChildParams extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(24, 0)
  ];
  String id = null;
  String displayNodeId = null;

  _ComposerUpdateChildParams() : super(kVersions.last.size);

  _ComposerUpdateChildParams.init(
    String this.id, 
    String this.displayNodeId
  ) : super(kVersions.last.size);

  static _ComposerUpdateChildParams deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static _ComposerUpdateChildParams decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    _ComposerUpdateChildParams result = new _ComposerUpdateChildParams();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.id = decoder0.decodeString(8, false);
    }
    if (mainDataHeader.version >= 0) {
      
      result.displayNodeId = decoder0.decodeString(16, false);
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    try {
      encoder0.encodeString(id, 8, false);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "id of struct _ComposerUpdateChildParams: $e";
      rethrow;
    }
    try {
      encoder0.encodeString(displayNodeId, 16, false);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "displayNodeId of struct _ComposerUpdateChildParams: $e";
      rethrow;
    }
  }

  String toString() {
    return "_ComposerUpdateChildParams("
           "id: $id" ", "
           "displayNodeId: $displayNodeId" ")";
  }

  Map toJson() {
    Map map = new Map();
    map["id"] = id;
    map["displayNodeId"] = displayNodeId;
    return map;
  }
}


class _ComposableDisplayParams extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(16, 0)
  ];
  String embodiment = null;

  _ComposableDisplayParams() : super(kVersions.last.size);

  _ComposableDisplayParams.init(
    String this.embodiment
  ) : super(kVersions.last.size);

  static _ComposableDisplayParams deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static _ComposableDisplayParams decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    _ComposableDisplayParams result = new _ComposableDisplayParams();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.embodiment = decoder0.decodeString(8, false);
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    try {
      encoder0.encodeString(embodiment, 8, false);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "embodiment of struct _ComposableDisplayParams: $e";
      rethrow;
    }
  }

  String toString() {
    return "_ComposableDisplayParams("
           "embodiment: $embodiment" ")";
  }

  Map toJson() {
    Map map = new Map();
    map["embodiment"] = embodiment;
    return map;
  }
}


class _ComposableBackParams extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(8, 0)
  ];

  _ComposableBackParams() : super(kVersions.last.size);

  _ComposableBackParams.init(
  ) : super(kVersions.last.size);

  static _ComposableBackParams deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static _ComposableBackParams decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    _ComposableBackParams result = new _ComposableBackParams();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    encoder.getStructEncoderAtOffset(kVersions.last);
  }

  String toString() {
    return "_ComposableBackParams("")";
  }

  Map toJson() {
    Map map = new Map();
    return map;
  }
}


class ComposableBackResponseParams extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(16, 0)
  ];
  bool wasHandled = false;

  ComposableBackResponseParams() : super(kVersions.last.size);

  ComposableBackResponseParams.init(
    bool this.wasHandled
  ) : super(kVersions.last.size);

  static ComposableBackResponseParams deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static ComposableBackResponseParams decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    ComposableBackResponseParams result = new ComposableBackResponseParams();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.wasHandled = decoder0.decodeBool(8, 0);
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    try {
      encoder0.encodeBool(wasHandled, 8, 0);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "wasHandled of struct ComposableBackResponseParams: $e";
      rethrow;
    }
  }

  String toString() {
    return "ComposableBackResponseParams("
           "wasHandled: $wasHandled" ")";
  }

  Map toJson() {
    Map map = new Map();
    map["wasHandled"] = wasHandled;
    return map;
  }
}

const int _composerMethodAddChildName = 0;
const int _composerMethodRemoveChildName = 1;
const int _composerMethodUpdateChildName = 2;

class _ComposerServiceDescription implements service_describer.ServiceDescription {
  void getTopLevelInterface(Function responder) {
    responder(null);
  }

  void getTypeDefinition(String typeKey, Function responder) {
    responder(null);
  }

  void getAllTypeDefinitions(Function responder) {
    responder(null);
  }
}

abstract class Composer {
  static const String serviceName = "modular::Composer";

  static service_describer.ServiceDescription _cachedServiceDescription;
  static service_describer.ServiceDescription get serviceDescription {
    if (_cachedServiceDescription == null) {
      _cachedServiceDescription = new _ComposerServiceDescription();
    }
    return _cachedServiceDescription;
  }

  static ComposerProxy connectToService(
      bindings.ServiceConnector s, String url, [String serviceName]) {
    ComposerProxy p = new ComposerProxy.unbound();
    String name = serviceName ?? Composer.serviceName;
    if ((name == null) || name.isEmpty) {
      throw new core.MojoApiError(
          "If an interface has no ServiceName, then one must be provided.");
    }
    s.connectToService(url, p, name);
    return p;
  }
  void addChild(String id, ModuleInstanceDisplayData moduleData);
  void removeChild(String id);
  void updateChild(String id, String displayNodeId);
}

abstract class ComposerInterface
    implements bindings.MojoInterface<Composer>,
               Composer {
  factory ComposerInterface([Composer impl]) =>
      new ComposerStub.unbound(impl);

  factory ComposerInterface.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint,
      [Composer impl]) =>
      new ComposerStub.fromEndpoint(endpoint, impl);

  factory ComposerInterface.fromMock(
      Composer mock) =>
      new ComposerProxy.fromMock(mock);
}

abstract class ComposerInterfaceRequest
    implements bindings.MojoInterface<Composer>,
               Composer {
  factory ComposerInterfaceRequest() =>
      new ComposerProxy.unbound();
}

class _ComposerProxyControl
    extends bindings.ProxyMessageHandler
    implements bindings.ProxyControl<Composer> {
  Composer impl;

  _ComposerProxyControl.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint) : super.fromEndpoint(endpoint);

  _ComposerProxyControl.fromHandle(
      core.MojoHandle handle) : super.fromHandle(handle);

  _ComposerProxyControl.unbound() : super.unbound();

  String get serviceName => Composer.serviceName;

  void handleResponse(bindings.ServiceMessage message) {
    switch (message.header.type) {
      default:
        proxyError("Unexpected message type: ${message.header.type}");
        close(immediate: true);
        break;
    }
  }

  @override
  String toString() {
    var superString = super.toString();
    return "_ComposerProxyControl($superString)";
  }
}

class ComposerProxy
    extends bindings.Proxy<Composer>
    implements Composer,
               ComposerInterface,
               ComposerInterfaceRequest {
  ComposerProxy.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint)
      : super(new _ComposerProxyControl.fromEndpoint(endpoint));

  ComposerProxy.fromHandle(core.MojoHandle handle)
      : super(new _ComposerProxyControl.fromHandle(handle));

  ComposerProxy.unbound()
      : super(new _ComposerProxyControl.unbound());

  factory ComposerProxy.fromMock(Composer mock) {
    ComposerProxy newMockedProxy =
        new ComposerProxy.unbound();
    newMockedProxy.impl = mock;
    return newMockedProxy;
  }

  static ComposerProxy newFromEndpoint(
      core.MojoMessagePipeEndpoint endpoint) {
    assert(endpoint.setDescription("For ComposerProxy"));
    return new ComposerProxy.fromEndpoint(endpoint);
  }


  void addChild(String id, ModuleInstanceDisplayData moduleData) {
    if (impl != null) {
      impl.addChild(id, moduleData);
      return;
    }
    if (!ctrl.isBound) {
      ctrl.proxyError("The Proxy is closed.");
      return;
    }
    var params = new _ComposerAddChildParams();
    params.id = id;
    params.moduleData = moduleData;
    ctrl.sendMessage(params,
        _composerMethodAddChildName);
  }
  void removeChild(String id) {
    if (impl != null) {
      impl.removeChild(id);
      return;
    }
    if (!ctrl.isBound) {
      ctrl.proxyError("The Proxy is closed.");
      return;
    }
    var params = new _ComposerRemoveChildParams();
    params.id = id;
    ctrl.sendMessage(params,
        _composerMethodRemoveChildName);
  }
  void updateChild(String id, String displayNodeId) {
    if (impl != null) {
      impl.updateChild(id, displayNodeId);
      return;
    }
    if (!ctrl.isBound) {
      ctrl.proxyError("The Proxy is closed.");
      return;
    }
    var params = new _ComposerUpdateChildParams();
    params.id = id;
    params.displayNodeId = displayNodeId;
    ctrl.sendMessage(params,
        _composerMethodUpdateChildName);
  }
}

class _ComposerStubControl
    extends bindings.StubMessageHandler
    implements bindings.StubControl<Composer> {
  Composer _impl;

  _ComposerStubControl.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint, [Composer impl])
      : super.fromEndpoint(endpoint, autoBegin: impl != null) {
    _impl = impl;
  }

  _ComposerStubControl.fromHandle(
      core.MojoHandle handle, [Composer impl])
      : super.fromHandle(handle, autoBegin: impl != null) {
    _impl = impl;
  }

  _ComposerStubControl.unbound([this._impl]) : super.unbound();

  String get serviceName => Composer.serviceName;



  void handleMessage(bindings.ServiceMessage message) {
    if (bindings.ControlMessageHandler.isControlMessage(message)) {
      bindings.ControlMessageHandler.handleMessage(
          this, 0, message);
      return;
    }
    if (_impl == null) {
      throw new core.MojoApiError("$this has no implementation set");
    }
    switch (message.header.type) {
      case _composerMethodAddChildName:
        var params = _ComposerAddChildParams.deserialize(
            message.payload);
        _impl.addChild(params.id, params.moduleData);
        break;
      case _composerMethodRemoveChildName:
        var params = _ComposerRemoveChildParams.deserialize(
            message.payload);
        _impl.removeChild(params.id);
        break;
      case _composerMethodUpdateChildName:
        var params = _ComposerUpdateChildParams.deserialize(
            message.payload);
        _impl.updateChild(params.id, params.displayNodeId);
        break;
      default:
        throw new bindings.MojoCodecError("Unexpected message name");
        break;
    }
  }

  Composer get impl => _impl;
  set impl(Composer d) {
    if (d == null) {
      throw new core.MojoApiError("$this: Cannot set a null implementation");
    }
    if (isBound && (_impl == null)) {
      beginHandlingEvents();
    }
    _impl = d;
  }

  @override
  void bind(core.MojoMessagePipeEndpoint endpoint) {
    super.bind(endpoint);
    if (!isOpen && (_impl != null)) {
      beginHandlingEvents();
    }
  }

  @override
  String toString() {
    var superString = super.toString();
    return "_ComposerStubControl($superString)";
  }

  int get version => 0;
}

class ComposerStub
    extends bindings.Stub<Composer>
    implements Composer,
               ComposerInterface,
               ComposerInterfaceRequest {
  ComposerStub.unbound([Composer impl])
      : super(new _ComposerStubControl.unbound(impl));

  ComposerStub.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint, [Composer impl])
      : super(new _ComposerStubControl.fromEndpoint(endpoint, impl));

  ComposerStub.fromHandle(
      core.MojoHandle handle, [Composer impl])
      : super(new _ComposerStubControl.fromHandle(handle, impl));

  static ComposerStub newFromEndpoint(
      core.MojoMessagePipeEndpoint endpoint) {
    assert(endpoint.setDescription("For ComposerStub"));
    return new ComposerStub.fromEndpoint(endpoint);
  }


  void addChild(String id, ModuleInstanceDisplayData moduleData) {
    return impl.addChild(id, moduleData);
  }
  void removeChild(String id) {
    return impl.removeChild(id);
  }
  void updateChild(String id, String displayNodeId) {
    return impl.updateChild(id, displayNodeId);
  }
}

const int _composableMethodDisplayName = 0;
const int _composableMethodBackName = 1;

class _ComposableServiceDescription implements service_describer.ServiceDescription {
  void getTopLevelInterface(Function responder) {
    responder(null);
  }

  void getTypeDefinition(String typeKey, Function responder) {
    responder(null);
  }

  void getAllTypeDefinitions(Function responder) {
    responder(null);
  }
}

abstract class Composable {
  static const String serviceName = "modular::Composable";

  static service_describer.ServiceDescription _cachedServiceDescription;
  static service_describer.ServiceDescription get serviceDescription {
    if (_cachedServiceDescription == null) {
      _cachedServiceDescription = new _ComposableServiceDescription();
    }
    return _cachedServiceDescription;
  }

  static ComposableProxy connectToService(
      bindings.ServiceConnector s, String url, [String serviceName]) {
    ComposableProxy p = new ComposableProxy.unbound();
    String name = serviceName ?? Composable.serviceName;
    if ((name == null) || name.isEmpty) {
      throw new core.MojoApiError(
          "If an interface has no ServiceName, then one must be provided.");
    }
    s.connectToService(url, p, name);
    return p;
  }
  void display(String embodiment);
  void back(void callback(bool wasHandled));
}

abstract class ComposableInterface
    implements bindings.MojoInterface<Composable>,
               Composable {
  factory ComposableInterface([Composable impl]) =>
      new ComposableStub.unbound(impl);

  factory ComposableInterface.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint,
      [Composable impl]) =>
      new ComposableStub.fromEndpoint(endpoint, impl);

  factory ComposableInterface.fromMock(
      Composable mock) =>
      new ComposableProxy.fromMock(mock);
}

abstract class ComposableInterfaceRequest
    implements bindings.MojoInterface<Composable>,
               Composable {
  factory ComposableInterfaceRequest() =>
      new ComposableProxy.unbound();
}

class _ComposableProxyControl
    extends bindings.ProxyMessageHandler
    implements bindings.ProxyControl<Composable> {
  Composable impl;

  _ComposableProxyControl.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint) : super.fromEndpoint(endpoint);

  _ComposableProxyControl.fromHandle(
      core.MojoHandle handle) : super.fromHandle(handle);

  _ComposableProxyControl.unbound() : super.unbound();

  String get serviceName => Composable.serviceName;

  void handleResponse(bindings.ServiceMessage message) {
    switch (message.header.type) {
      case _composableMethodBackName:
        var r = ComposableBackResponseParams.deserialize(
            message.payload);
        if (!message.header.hasRequestId) {
          proxyError("Expected a message with a valid request Id.");
          return;
        }
        Function callback = callbackMap[message.header.requestId];
        if (callback == null) {
          proxyError(
              "Message had unknown request Id: ${message.header.requestId}");
          return;
        }
        callbackMap.remove(message.header.requestId);
        callback(r.wasHandled );
        break;
      default:
        proxyError("Unexpected message type: ${message.header.type}");
        close(immediate: true);
        break;
    }
  }

  @override
  String toString() {
    var superString = super.toString();
    return "_ComposableProxyControl($superString)";
  }
}

class ComposableProxy
    extends bindings.Proxy<Composable>
    implements Composable,
               ComposableInterface,
               ComposableInterfaceRequest {
  ComposableProxy.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint)
      : super(new _ComposableProxyControl.fromEndpoint(endpoint));

  ComposableProxy.fromHandle(core.MojoHandle handle)
      : super(new _ComposableProxyControl.fromHandle(handle));

  ComposableProxy.unbound()
      : super(new _ComposableProxyControl.unbound());

  factory ComposableProxy.fromMock(Composable mock) {
    ComposableProxy newMockedProxy =
        new ComposableProxy.unbound();
    newMockedProxy.impl = mock;
    return newMockedProxy;
  }

  static ComposableProxy newFromEndpoint(
      core.MojoMessagePipeEndpoint endpoint) {
    assert(endpoint.setDescription("For ComposableProxy"));
    return new ComposableProxy.fromEndpoint(endpoint);
  }


  void display(String embodiment) {
    if (impl != null) {
      impl.display(embodiment);
      return;
    }
    if (!ctrl.isBound) {
      ctrl.proxyError("The Proxy is closed.");
      return;
    }
    var params = new _ComposableDisplayParams();
    params.embodiment = embodiment;
    ctrl.sendMessage(params,
        _composableMethodDisplayName);
  }
  void back(void callback(bool wasHandled)) {
    if (impl != null) {
      impl.back(callback);
      return;
    }
    var params = new _ComposableBackParams();
    Function zonedCallback;
    if (identical(Zone.current, Zone.ROOT)) {
      zonedCallback = callback;
    } else {
      Zone z = Zone.current;
      zonedCallback = ((bool wasHandled) {
        z.bindCallback(() {
          callback(wasHandled);
        })();
      });
    }
    ctrl.sendMessageWithRequestId(
        params,
        _composableMethodBackName,
        -1,
        bindings.MessageHeader.kMessageExpectsResponse,
        zonedCallback);
  }
}

class _ComposableStubControl
    extends bindings.StubMessageHandler
    implements bindings.StubControl<Composable> {
  Composable _impl;

  _ComposableStubControl.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint, [Composable impl])
      : super.fromEndpoint(endpoint, autoBegin: impl != null) {
    _impl = impl;
  }

  _ComposableStubControl.fromHandle(
      core.MojoHandle handle, [Composable impl])
      : super.fromHandle(handle, autoBegin: impl != null) {
    _impl = impl;
  }

  _ComposableStubControl.unbound([this._impl]) : super.unbound();

  String get serviceName => Composable.serviceName;


  Function _composableBackResponseParamsResponder(
      int requestId) {
  return (bool wasHandled) {
      var result = new ComposableBackResponseParams();
      result.wasHandled = wasHandled;
      sendResponse(buildResponseWithId(
          result,
          _composableMethodBackName,
          requestId,
          bindings.MessageHeader.kMessageIsResponse));
    };
  }

  void handleMessage(bindings.ServiceMessage message) {
    if (bindings.ControlMessageHandler.isControlMessage(message)) {
      bindings.ControlMessageHandler.handleMessage(
          this, 0, message);
      return;
    }
    if (_impl == null) {
      throw new core.MojoApiError("$this has no implementation set");
    }
    switch (message.header.type) {
      case _composableMethodDisplayName:
        var params = _ComposableDisplayParams.deserialize(
            message.payload);
        _impl.display(params.embodiment);
        break;
      case _composableMethodBackName:
        _impl.back(_composableBackResponseParamsResponder(message.header.requestId));
        break;
      default:
        throw new bindings.MojoCodecError("Unexpected message name");
        break;
    }
  }

  Composable get impl => _impl;
  set impl(Composable d) {
    if (d == null) {
      throw new core.MojoApiError("$this: Cannot set a null implementation");
    }
    if (isBound && (_impl == null)) {
      beginHandlingEvents();
    }
    _impl = d;
  }

  @override
  void bind(core.MojoMessagePipeEndpoint endpoint) {
    super.bind(endpoint);
    if (!isOpen && (_impl != null)) {
      beginHandlingEvents();
    }
  }

  @override
  String toString() {
    var superString = super.toString();
    return "_ComposableStubControl($superString)";
  }

  int get version => 0;
}

class ComposableStub
    extends bindings.Stub<Composable>
    implements Composable,
               ComposableInterface,
               ComposableInterfaceRequest {
  ComposableStub.unbound([Composable impl])
      : super(new _ComposableStubControl.unbound(impl));

  ComposableStub.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint, [Composable impl])
      : super(new _ComposableStubControl.fromEndpoint(endpoint, impl));

  ComposableStub.fromHandle(
      core.MojoHandle handle, [Composable impl])
      : super(new _ComposableStubControl.fromHandle(handle, impl));

  static ComposableStub newFromEndpoint(
      core.MojoMessagePipeEndpoint endpoint) {
    assert(endpoint.setDescription("For ComposableStub"));
    return new ComposableStub.fromEndpoint(endpoint);
  }


  void display(String embodiment) {
    return impl.display(embodiment);
  }
  void back(void callback(bool wasHandled)) {
    return impl.back(callback);
  }
}



