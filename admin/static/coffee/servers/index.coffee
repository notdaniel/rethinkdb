# Copyright 2010-2012 RethinkDB, all rights reserved.
module 'ServersView', ->
    class @ServersContainer extends Backbone.View
        id: 'servers_container'
        template: Handlebars.templates['servers_container-template']

        initialize: =>
            @servers = new Servers
            @servers_list = new ServersView.ServersListView
                collection: @servers

            @fetch_servers()
            @interval = setInterval @fetch_servers, 5000

            @loading = true # TODO Render that

        render: =>
            @$el.html @template({})
            @$('.servers_list').html @servers_list.render().$el
            return @

        fetch_servers: =>
            #TODO Replace later with server_status
            query = r.db(system_db).table('server_config').merge( (server) ->
                id: server("uuid")
                primary_count: r.db(system_db).table('table_config').concatMap( (table) -> table("shards") ).filter( (shard) -> shard("directors").nth(0).eq(server("name")) ).count()
                secondary_count: r.db(system_db).table('table_config').concatMap( (table) -> table("shards") ).filter( (shard) -> shard("replicas").nth(0).eq(server("name")) ).count()
                status:
                    reachable: true
                    last_seen: $.timeago(new Date())
            )
            driver.run query, (error, result) =>
                @loading = false

                uuids = {}
                for server, index in result
                    @servers.add new Server(server)
                    uuids[server.id] = true

                # Clean  removed servers
                toDestroy = []
                for server in @servers.models
                    if uuids[server.get('id')] isnt true
                        toDestroy.push server
                for server in toDestroy
                    server.destroy()

        remove: =>
            clearInterval @interval
            @servers_list.remove()
            super()

    class @ServersListView extends Backbone.View
        className: 'servers_view'
        initialize: =>
            @servers_view = []
            @collection.each (server) =>
                view = new ServersView.ServerView
                    model: server

                @servers_view.push view
                @$el.append view.render().$el

            @listenTo @collection, 'add', (server) =>
                view = new ServersView.ServerView
                    model: server

                @servers_view.push view
                @$el.append view.render().$el

            ###
            TODO: Implement this once table_status and remove_server is available
            @collection.on 'remove', (server) =>
            ###

        render: =>
            for server_view in @servers_view
                @$el.append server_view.render().$el
            @

        remove: =>
            @stopListening()
            for view in @servers_view
                view.remove()
            super()

    class @ServerView extends Backbone.View
        className: 'server_view'
        template: Handlebars.templates['server-template']
        initialize: =>
            @listenTo @model, 'change', @render

        render: ->
            @$el.html @template @model.toJSON()
            @

        remove: =>
            @stopListening()
