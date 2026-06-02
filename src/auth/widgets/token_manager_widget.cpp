#include "token_manager_widget.hpp"

#include <Wt/WDialog.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include <utility>

token_manager_widget::token_manager_widget(user_db& db, std::string username, long long actor_id):
  m_db{db},
  m_username{std::move(username)},
  m_actor_id{actor_id}
{
	setStyleClass("account-tokens-section");
	addNew<Wt::WText>("<h3>API Tokens</h3>", Wt::TextFormat::UnsafeXHTML);

	m_list = addNew<Wt::WContainerWidget>();
	build_list();

	auto* gen_btn = addNew<Wt::WPushButton>("Generate New Token");
	gen_btn->setStyleClass("editor-btn");
	gen_btn->clicked().connect(this, &token_manager_widget::generate);
}

void token_manager_widget::build_list()
{
	m_list->clear();

	const auto tokens = m_db.list_tokens(m_username);
	if(tokens.empty())
	{
		m_list->addNew<Wt::WText>("No tokens.", Wt::TextFormat::Plain)
		  ->setStyleClass("account-tokens-empty");
		return;
	}

	for(const auto& tok: tokens)
	{
		auto* row = m_list->addNew<Wt::WContainerWidget>();
		row->setStyleClass("account-token-row");

		row->addNew<Wt::WText>(tok.name, Wt::TextFormat::Plain)
		  ->setStyleClass("account-token-name");

		const long long token_id = tok.id;
		const auto      name     = tok.name;

		auto* rename_btn = row->addNew<Wt::WPushButton>("Rename");
		rename_btn->setStyleClass("link-action-btn");
		rename_btn->clicked().connect([this, token_id, name] { rename(token_id, name); });

		auto* rev_btn = row->addNew<Wt::WPushButton>("Revoke");
		rev_btn->setStyleClass("link-action-btn link-delete-btn");
		rev_btn->clicked().connect([this, token_id] { revoke(token_id); });
	}
}

void token_manager_widget::generate()
{
	auto* d = new Wt::WDialog("New API Token");
	d->contents()->addNew<Wt::WText>("<p>Name this token so you can recognize it later.</p>",
	                                 Wt::TextFormat::UnsafeXHTML);
	auto* name_field = d->contents()->addNew<Wt::WLineEdit>();
	name_field->setStyleClass("editor-field");
	name_field->setPlaceholderText("Token name");

	auto* create = d->footer()->addNew<Wt::WPushButton>("Generate");
	create->setStyleClass("editor-btn");
	auto* cancel = d->footer()->addNew<Wt::WPushButton>("Cancel");
	cancel->setStyleClass("editor-btn editor-btn-cancel");

	create->clicked().connect([this, d, name_field] {
		const auto name = name_field->text().toUTF8();
		if(name.empty())
		{
			name_field->setPlaceholderText("A name is required");
			return;
		}
		const auto raw_token = m_db.create_api_token(m_username, name, m_actor_id);
		d->accept();
		build_list();

		// Reveal the raw token once.
		auto* reveal = new Wt::WDialog("Token Created");
		reveal->contents()->addNew<Wt::WText>(
		  "<p>Copy this token now — it will <strong>not</strong> be shown again.</p>",
		  Wt::TextFormat::UnsafeXHTML);
		auto* field = reveal->contents()->addNew<Wt::WLineEdit>(raw_token);
		field->setReadOnly(true);
		field->setStyleClass("editor-field token-display-field");
		auto* ok = reveal->footer()->addNew<Wt::WPushButton>("Done");
		ok->setStyleClass("editor-btn");
		ok->clicked().connect([reveal] { reveal->accept(); });
		reveal->finished().connect([reveal](Wt::DialogCode) { delete reveal; });
		reveal->show();
	});
	cancel->clicked().connect([d] { d->reject(); });
	d->finished().connect([d](Wt::DialogCode) { delete d; });
	d->show();
}

void token_manager_widget::rename(long long token_id, const std::string& current_name)
{
	auto* d     = new Wt::WDialog("Rename Token");
	auto* field = d->contents()->addNew<Wt::WLineEdit>(current_name);
	field->setStyleClass("editor-field");

	auto* save = d->footer()->addNew<Wt::WPushButton>("Save");
	save->setStyleClass("editor-btn");
	auto* cancel = d->footer()->addNew<Wt::WPushButton>("Cancel");
	cancel->setStyleClass("editor-btn editor-btn-cancel");

	save->clicked().connect([this, d, field, token_id] {
		const auto name = field->text().toUTF8();
		if(name.empty())
		{
			return;
		}
		d->accept();
		m_db.rename_api_token(token_id, name, m_actor_id);
		build_list();
	});
	cancel->clicked().connect([d] { d->reject(); });
	d->finished().connect([d](Wt::DialogCode) { delete d; });
	d->show();
}

void token_manager_widget::revoke(long long token_id)
{
	auto* d = new Wt::WDialog("Revoke Token");
	d->contents()->addNew<Wt::WText>("Revoke this API token? This cannot be undone.",
	                                 Wt::TextFormat::Plain);
	auto* yes = d->footer()->addNew<Wt::WPushButton>("Revoke");
	yes->setStyleClass("editor-btn editor-btn-danger");
	auto* no = d->footer()->addNew<Wt::WPushButton>("Cancel");
	no->setStyleClass("editor-btn editor-btn-cancel");

	yes->clicked().connect([this, d, token_id] {
		d->accept();
		m_db.delete_token(token_id, m_actor_id);
		build_list();
	});
	no->clicked().connect([d] { d->reject(); });
	d->finished().connect([d](Wt::DialogCode) { delete d; });
	d->show();
}
