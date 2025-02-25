/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/info_layer_widget.h"

#include "info/info_content_widget.h"
#include "info/info_top_bar.h"
#include "info/info_memento.h"
#include "ui/rp_widget.h"
#include "ui/focus_persister.h"
#include "ui/widgets/buttons.h"
#include "ui/cached_round_corners.h"
#include "window/section_widget.h"
#include "window/window_session_controller.h"
#include "window/main_window.h"
#include "main/main_session.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "styles/style_info.h"
#include "styles/style_window.h"
#include "styles/style_layers.h"

namespace Info {

LayerWidget::LayerWidget(
	not_null<Window::SessionController*> controller,
	not_null<Memento*> memento)
: _controller(controller)
, _content(this, controller, Wrap::Layer, memento) {
	setupHeightConsumers();
	Core::App().replaceFloatPlayerDelegate(floatPlayerDelegate());
}

LayerWidget::LayerWidget(
	not_null<Window::SessionController*> controller,
	not_null<MoveMemento*> memento)
: _controller(controller)
, _content(memento->takeContent(this, Wrap::Layer)) {
	setupHeightConsumers();
	Core::App().replaceFloatPlayerDelegate(floatPlayerDelegate());
}

auto LayerWidget::floatPlayerDelegate()
-> not_null<::Media::Player::FloatDelegate*> {
	return static_cast<::Media::Player::FloatDelegate*>(this);
}

not_null<Ui::RpWidget*> LayerWidget::floatPlayerWidget() {
	return this;
}

auto LayerWidget::floatPlayerGetSection(Window::Column column)
-> not_null<::Media::Player::FloatSectionDelegate*> {
	Expects(_content != nullptr);

	return _content;
}

void LayerWidget::floatPlayerEnumerateSections(Fn<void(
		not_null<::Media::Player::FloatSectionDelegate*> widget,
		Window::Column widgetColumn)> callback) {
	Expects(_content != nullptr);

	callback(_content, Window::Column::Second);
}

bool LayerWidget::floatPlayerIsVisible(not_null<HistoryItem*> item) {
	return false;
}

void LayerWidget::floatPlayerDoubleClickEvent(
		not_null<const HistoryItem*> item) {
	_controller->showPeerHistoryAtItem(item);
}

void LayerWidget::setupHeightConsumers() {
	Expects(_content != nullptr);

	_content->scrollTillBottomChanges(
	) | rpl::filter([this] {
		return !_inResize;
	}) | rpl::start_with_next([this] {
		resizeToWidth(width());
	}, lifetime());

	_content->grabbingForExpanding(
	) | rpl::start_with_next([=](bool grabbing) {
		if (grabbing) {
			_savedHeight = _contentHeight;
			_savedHeightAnimation = base::take(_heightAnimation);
			setContentHeight(_desiredHeight);
		} else {
			_heightAnimation = base::take(_savedHeightAnimation);
			setContentHeight(_savedHeight);
		}
	}, lifetime());

	_content->desiredHeightValue(
	) | rpl::start_with_next([this](int height) {
		if (!height) {
			// New content arrived.
			_heightAnimated = _heightAnimation.animating();
			return;
		}
		std::swap(_desiredHeight, height);
		if (!height
			|| (_heightAnimated && !_heightAnimation.animating())) {
			_heightAnimated = true;
			setContentHeight(_desiredHeight);
		} else {
			_heightAnimated = true;
			_heightAnimation.start([=] {
				setContentHeight(_heightAnimation.value(_desiredHeight));
			}, _contentHeight, _desiredHeight, st::slideDuration);
			resizeToWidth(width());
		}
	}, lifetime());
}

void LayerWidget::setContentHeight(int height) {
	if (_contentHeight == height) {
		return;
	}
	_contentHeight = height;
	if (_content && !_inResize) {
		resizeToWidth(width());
	}
}

void LayerWidget::showFinished() {
	floatPlayerShowVisible();
}

void LayerWidget::parentResized() {
	if (!_content) {
		return;
	}

	auto parentSize = parentWidget()->size();
	auto parentWidth = parentSize.width();
	if (parentWidth < MinimalSupportedWidth()) {
		Ui::FocusPersister persister(this);
		restoreFloatPlayerDelegate();

		auto memento = std::make_shared<MoveMemento>(std::move(_content));

		// We want to call hideSpecialLayer synchronously to avoid glitches,
		// but we can't destroy LayerStackWidget from its' resizeEvent,
		// because QWidget has such code for resizing:
		//
		// QResizeEvent e(r.size(), olds);
		// QApplication::sendEvent(q, &e);
		// if (q->windowHandle())
		//   q->update();
		//
		// So we call it queued. It would be cool to call it 'right after'
		// the resize event handling was finished.
		InvokeQueued(this, [=] {
			_controller->hideSpecialLayer(anim::type::instant);
		});
		_controller->showSection(
			std::move(memento),
			Window::SectionShow(
				Window::SectionShow::Way::Forward,
				anim::type::instant,
				anim::activation::background));
	//
	// There was a layout logic which caused layer info to become a
	// third column info if the window size allows, but it was decided
	// to keep layer info and third column info separated.
	//
	//} else if (_controller->canShowThirdSectionWithoutResize()) {
	//	takeToThirdSection();
	} else {
		auto newWidth = qMin(
			parentWidth - 2 * st::infoMinimalLayerMargin,
			st::infoDesiredWidth);
		resizeToWidth(newWidth);
	}
}

bool LayerWidget::takeToThirdSection() {
	return false;
	//
	// There was a layout logic which caused layer info to become a
	// third column info if the window size allows, but it was decided
	// to keep layer info and third column info separated.
	//
	//Ui::FocusPersister persister(this);
	//auto localCopy = _controller;
	//auto memento = MoveMemento(std::move(_content));
	//localCopy->hideSpecialLayer(anim::type::instant);

	//// When creating third section in response to the window
	//// size allowing it to fit without window resize we want
	//// to save that we didn't extend the window while showing
	//// the third section, so that when we close it we won't
	//// shrink the window size.
	////
	//// See https://github.com/telegramdesktop/tdesktop/issues/4091
	//localCopy->session()().settings().setThirdSectionExtendedBy(0);

	//localCopy->session()().settings().setThirdSectionInfoEnabled(true);
	//localCopy->session()().saveSettingsDelayed();
	//localCopy->showSection(
	//	std::move(memento),
	//	Window::SectionShow(
	//		Window::SectionShow::Way::ClearStack,
	//		anim::type::instant,
	//		anim::activation::background));
	//return true;
}

bool LayerWidget::showSectionInternal(
		not_null<Window::SectionMemento*> memento,
		const Window::SectionShow &params) {
	if (_content && _content->showInternal(memento, params)) {
		if (params.activation != anim::activation::background) {
			Ui::hideLayer();
		}
		return true;
	}
	return false;
}

bool LayerWidget::closeByOutsideClick() const {
	return _content ? _content->closeByOutsideClick() : true;
}

int LayerWidget::MinimalSupportedWidth() {
	const auto minimalMargins = 2 * st::infoMinimalLayerMargin;
	return st::infoMinimalWidth + minimalMargins;
}

int LayerWidget::resizeGetHeight(int newWidth) {
	if (!parentWidget() || !_content) {
		return 0;
	}
	_inResize = true;
	auto guard = gsl::finally([&] { _inResize = false; });

	auto parentSize = parentWidget()->size();
	auto windowWidth = parentSize.width();
	auto windowHeight = parentSize.height();
	auto newLeft = (windowWidth - newWidth) / 2;
	auto newTop = std::clamp(
		windowHeight / 24,
		st::infoLayerTopMinimal,
		st::infoLayerTopMaximal);
	auto newBottom = newTop;

	// Top rounding is included in _contentHeight.
	auto desiredHeight = _contentHeight + st::boxRadius;
	accumulate_min(desiredHeight, windowHeight - newTop - newBottom);

	// First resize content to new width and get the new desired height.
	auto contentLeft = 0;
	auto contentTop = 0;
	auto contentBottom = st::boxRadius;
	auto contentWidth = newWidth;
	auto contentHeight = desiredHeight - contentTop - contentBottom;
	auto scrollTillBottom = _content->scrollTillBottom(contentHeight);
	auto additionalScroll = std::min(scrollTillBottom, newBottom);

	const auto expanding = (_desiredHeight > _contentHeight);

	desiredHeight += additionalScroll;
	contentHeight += additionalScroll;
	_tillBottom = (newTop + desiredHeight >= windowHeight);
	if (_tillBottom) {
		contentHeight += contentBottom;
		additionalScroll += contentBottom;
	}
	_content->updateGeometry({
		contentLeft,
		contentTop,
		contentWidth,
		contentHeight,
	}, expanding, additionalScroll);

	auto newGeometry = QRect(newLeft, newTop, newWidth, desiredHeight);
	if (newGeometry != geometry()) {
		_content->forceContentRepaint();
	}
	if (newGeometry.topLeft() != geometry().topLeft()) {
		move(newGeometry.topLeft());
	}

	floatPlayerUpdatePositions();
	return desiredHeight;
}

void LayerWidget::doSetInnerFocus() {
	if (_content) {
		_content->setInnerFocus();
	}
}

void LayerWidget::paintEvent(QPaintEvent *e) {
	Painter p(this);

	const auto clip = e->rect();
	const auto radius = st::boxRadius;
	auto parts = RectPart::None | 0;
	if (!_tillBottom) {
		if (clip.intersects({ 0, height() - radius, width(), radius })) {
			parts |= RectPart::FullBottom;
		}
	}
	if (_content->animatingShow()) {
		if (clip.intersects({ 0, 0, width(), radius })) {
			parts |= RectPart::FullTop;
		}
		parts |= RectPart::Left | RectPart::Center | RectPart::Right;
	}
	if (parts) {
		Ui::FillRoundRect(
			p,
			rect(),
			st::boxBg,
			Ui::BoxCorners,
			nullptr,
			parts);
	}
}

void LayerWidget::restoreFloatPlayerDelegate() {
	if (!_floatPlayerDelegateRestored) {
		_floatPlayerDelegateRestored = true;
		Core::App().restoreFloatPlayerDelegate(floatPlayerDelegate());
	}
}

void LayerWidget::closeHook() {
	restoreFloatPlayerDelegate();
}

LayerWidget::~LayerWidget() {
	if (!Core::Quitting()) {
		restoreFloatPlayerDelegate();
	}
}

} // namespace Info
